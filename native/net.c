/* Netplay (pulseman's mdrt/netplay.h over mdrt/net_udp.h): two players, each on their own
 * machine, the host pad 1 and the guest pad 2, from power on. The game's resumable point is the
 * game loop's wait (setvideo's, where states are taken: state.c), and a rollback goes on from there
 * as a loaded state does (restart_at native_resume_game_loop). The game plays whatever session its
 * console has (mdrt_get_net): the window's, made on its Online page or from the command line, or a
 * headless run's own. The identity the peers compare: the build (its commit), the ROM, and what
 * changes the simulation on the command line (--rule, --league, --four-way). Each player's
 * buttons are their own controls' (the window's players in turn; headless, their pad's script).
 * Headless, a run that reaches --frames waits there until both have every button up to it, then a
 * second more for the other, then says goodbye.
 *
 * mdrt_net_frame is called at every wait before the handshake, as states are taken: a frame's
 * game code, then the session's turn, then the handshake reads the next frame's buttons. */
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "native.h"
#include "mdrt/net_udp.h"
#include "build_id.h"

static mdrt_udp *udp;                   /* the command line's session (net) and its link */
static mdrt_net *net;
static mdrt *console;
static int headless, started;           /* (headless: this file connects, starts and ends the session) */
static native_input_fn local_fn;
static long stop_at = -1;

static void local_input(void *user, uint16_t b[MDRT_PADS])
{
    (void)user;
    uint8_t pads = mdrt_net_pads(net);
    int k = 0;
    for (int p = 0; p < MDRT_PADS; p++)
        if (pads >> p & 1) { b[k] = local_fn ? local_fn(p, k) : 0; k++; }
}

const char *native_net_identity(const char *setup)
{
    static char identity[256];
    uint64_t h = 0xCBF29CE484222325ull;                   /* (the command line's part, hashed: the string stays short) */
    for (const char *s = setup ? setup : ""; *s; s++) h = (h ^ (uint8_t)*s) * 0x100000001B3ull;
    snprintf(identity, sizeof identity, "ninety-four %s, NHL 94 (UE) %08x, setup %016llx", NHL94_BUILD, NATIVE_ROM_CRC32,
             (unsigned long long)h);
    return identity;
}

int native_net_open(const mdrt_net_options *o, const char *setup, native_input_fn input, char *err, size_t errsize)
{
    udp = mdrt_net_open_udp(o, err, errsize);
    if (!udp) return 0;
    mdrt_net_config c = { .role = o->role, .identity = native_net_identity(setup), .name = o->name, .delay = o->delay,
                          .window = 8, .rollback = 30, .check_every = 60 };
    c.link = mdrt_udp_link(udp);
    c.local_input = local_input;
    local_fn = input;
    net = mdrt_net_new(&c);
    if (!net) { snprintf(err, errsize, "a bad configuration"); mdrt_udp_close(udp); udp = NULL; return 0; }
    return 1;
}

/* the session the game plays in now, while it lasts */
static mdrt_net *playing(void)
{
    mdrt_net *n = console ? mdrt_get_net(console) : NULL;
    return n && mdrt_net_state(n) == MDRT_NET_GO ? n : NULL;
}

int native_net_on(void) { return playing() != NULL || (headless && !started); }

/* headless: connect, then the game powers on (native_net_start) */
int native_net_connect(void)
{
    char peer[96];
    fprintf(stderr, "netplay: on port %u, connecting\n", (unsigned)mdrt_udp_port(udp));
    for (;;) {
        int r = mdrt_net_connect(net);
        if (r == MDRT_NET_READY) break;
        if (r != MDRT_NET_CONNECTING) { fprintf(stderr, "netplay: %s\n", mdrt_net_why(net)); return 0; }
        mdrt_udp_wait(udp, 10);
    }
    mdrt_udp_lock(udp);
    headless = 1;
    fprintf(stderr, "netplay: playing with %s, %s, pad %d, input delay %d\n", mdrt_udp_peer(udp, peer, sizeof peer),
            mdrt_net_is_host(net) ? "hosting" : "joined", mdrt_net_pads(net) == 1 ? 1 : 2, mdrt_net_delay(net));
    const char *who = mdrt_net_peer_name(net);
    fprintf(stderr, "netplay: %s has joined\n", who[0] ? who : mdrt_net_is_host(net) ? "player 2" : "player 1");
    return 1;
}

mdrt_net *native_net(void) { return net; }
mdrt_udp *native_net_udp(void) { return udp; }

/* a console powered on (its states' parts added): in the window, its session (if any) is the
 * window's, started already; headless, this run's own, from this power on */
void native_net_start(mdrt *m)
{
    console = m;
    if (!headless || started || mdrt_get_net(m)) return;
    mdrt_set_net(m, net);
    mdrt_net_start(net, m);
    started = 1;
}

void native_net_stop_at(long frame) { stop_at = frame; }

static void report(mdrt_net *n)
{
    mdrt_net_stats s;
    mdrt_net_stats_get(n, &s);
    fprintf(stderr, "netplay: frame %u, %ld rollbacks (%ld frames played again, at most %d), %ld frames given back, %ld desync checks, round trip %d ms\n",
            (unsigned)s.frame, s.rollbacks, s.replayed, s.most_replayed, s.gave_back, s.checks, s.rtt_ms);
}

static void over(mdrt_net *n)
{
    if (headless) fprintf(stderr, "netplay: %s\n", mdrt_net_why(n));   /* (the window says why itself) */
    report(n);
}

/* the end of a headless run: every button up to the last frame known here and there, then a
 * second for the other */
static void finish(mdrt_net *n, int resumable)
{
    uint64_t linger = 0;
    for (;;) {
        int r = mdrt_net_frame(n, resumable);
        if (r == MDRT_NET_RESTART) restart_at(native_resume_game_loop);
        mdrt_net_stats s;
        mdrt_net_stats_get(n, &s);
        if (r == MDRT_NET_ENDED || r == MDRT_NET_REFUSED) {   /* (the other done first, or gone) */
            if (s.confirmed < (uint32_t)stop_at) over(n);
            else report(n);
            native_quit();
        }
        mdrt_net_link l = mdrt_udp_link(udp);
        if (!linger && s.confirmed >= (uint32_t)stop_at && s.remote_has >= (uint32_t)stop_at && s.remote_frame >= (uint32_t)stop_at)
            linger = l.ticks(l.user) + 1000000u;
        if (linger && l.ticks(l.user) >= linger) break;
        mdrt_udp_wait(udp, 1);
    }
    report(n);
    mdrt_net_quit(n);
    native_quit();
}

/* at every wait, before the handshake (a rollback: the game loop from its wait, as a state loaded) */
void native_net_wait(void)
{
    mdrt_net *n = playing();
    if (!n) return;                                       /* (none, or over: the console plays on alone) */
    int resumable = native_at_game_loop_wait(), r;
    while ((r = mdrt_net_frame(n, resumable)) == MDRT_NET_WAIT) {
        if (mdrt_idle(console)) {                         /* (the window closing: goodbye) */
            mdrt_net_quit(n);
            native_quit();
        }
        if (headless) mdrt_udp_wait(udp, 1);              /* (the window waits in mdrt_idle) */
    }
    if (r == MDRT_NET_RESTART) restart_at(native_resume_game_loop);
    if (r == MDRT_NET_ENDED || r == MDRT_NET_REFUSED) { over(n); return; }
    if (headless && stop_at >= 0 && native_frames() >= stop_at && !mdrt_quiet(console)) finish(n, resumable);
}

void native_net_close(void)
{
    if (net && mdrt_net_state(net) == MDRT_NET_GO) { report(net); fprintf(stderr, "netplay: goodbye\n"); }
    if (net) mdrt_net_quit(net);
    mdrt_net_free(net);
    mdrt_udp_close(udp);
    net = NULL;
    udp = NULL;
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "digs_lines.h"

/*
 * Lines are grouped into sets and the index points at sets, so no offset into
 * a flat blob is ever written by hand.  A global line id is the set number
 * times DIGS_LINE_SET_STRIDE plus the position within the set, which gives the
 * recent-line ring a stable small integer to remember without anybody having
 * to keep a running total in their head.
 *
 * %T is the only substitution, and it means "the miner being talked about".
 */
#define DIGS_LINE_SET_STRIDE 256U

/* ---- what anyone might say ------------------------------------------- */

static const char *const digs_any_first_meeting[] = {
    "SO THERE'S SOMEBODY ELSE DOWN HERE.",
    "%T. RIGHT.",
    "DIDN'T THINK I HAD COMPANY.",
    "WELL. HELLO, %T."
};

static const char *const digs_any_spotted[] = {
    "THERE YOU ARE.",
    "I SEE YOU, %T.",
    "FOUND YOU.",
    "DON'T MOVE."
};

static const char *const digs_any_hurt_them[] = {
    "THAT ONE LANDED.",
    "FELT THAT, %T?",
    "HOLD STILL.",
    "GETTING CLOSER."
};

static const char *const digs_any_hurt_by[] = {
    "AGH!",
    "THAT'S COMING OUT OF YOUR PAY, %T.",
    "LUCKY SHOT.",
    "ALL RIGHT. ALL RIGHT."
};

static const char *const digs_any_near_miss[] = {
    "MISSED.",
    "NOT EVEN CLOSE, %T.",
    "TRY AIMING.",
    "WIDE."
};

static const char *const digs_any_limb_taken[] = {
    "YOU'LL WANT THAT BACK.",
    "THAT CAME OFF EASY.",
    "ONE LESS FOR YOU, %T.",
    "OOPS."
};

static const char *const digs_any_limb_lost[] = {
    "MY ARM! %T, MY ARM!",
    "THAT WAS ATTACHED!",
    "I NEEDED THAT.",
    "STILL STANDING."
};

static const char *const digs_any_killed_them[] = {
    "DOWN.",
    "SHIFT'S OVER, %T.",
    "THAT'S THAT.",
    "NEXT."
};

static const char *const digs_any_killed_by[] = {
    "%T GOT ME.",
    "NOT LIKE THIS.",
    "I'LL REMEMBER THAT.",
    "TELL MY SUPERVISOR."
};

static const char *const digs_any_revenge[] = {
    "WE'RE EVEN, %T.",
    "THAT WAS FOR EARLIER.",
    "SQUARE NOW.",
    "PAID IN FULL."
};

static const char *const digs_any_humiliated[] = {
    "AGAIN? REALLY?",
    "%T, THIS IS GETTING OLD.",
    "STOP DOING THAT.",
    "I HATE THIS SHIFT."
};

static const char *const digs_any_streak[] = {
    "ANYONE ELSE?",
    "LINE UP.",
    "I'M HAVING A GOOD DAY.",
    "WHO'S NEXT."
};

static const char *const digs_any_saved_by[] = {
    "THAT WAS DECENT OF YOU, %T.",
    "I OWE YOU ONE.",
    "DIDN'T EXPECT THAT.",
    "HUH. THANKS."
};

static const char *const digs_any_teamed_up[] = {
    "NICE WORK, %T.",
    "WE MAKE A TEAM.",
    "AGAIN, SAME AS THAT.",
    "KEEP IT UP."
};

static const char *const digs_any_betrayed[] = {
    "YOU. YOU ACTUALLY DID THAT.",
    "WE HAD AN UNDERSTANDING, %T!",
    "I TRUSTED YOU.",
    "THAT'S THE LAST TIME."
};

static const char *const digs_any_truce_offered[] = {
    "TRUCE, %T?",
    "WE COULD BOTH WALK AWAY.",
    "I'M NOT SHOOTING. YOUR MOVE.",
    "ENOUGH?"
};

static const char *const digs_any_truce_accepted[] = {
    "ALL RIGHT. TRUCE.",
    "DEAL, %T.",
    "I CAN LIVE WITH THAT.",
    "FINE. NO SHOOTING."
};

static const char *const digs_any_truce_broken[] = {
    "SO MUCH FOR THAT.",
    "YOU BROKE IT, %T.",
    "I KNEW IT.",
    "BACK TO WORK, THEN."
};

static const char *const digs_any_taunted[] = {
    "SAY THAT AGAIN.",
    "BIG WORDS, %T.",
    "UH HUH.",
    "KEEP TALKING."
};

static const char *const digs_any_lava_close[] = {
    "IT'S COMING UP!",
    "THAT'S THE LAVA!",
    "MOVE, MOVE!",
    "GETTING WARM."
};

static const char *const digs_any_buried[] = {
    "I'M STUCK!",
    "SOMETHING'S ON TOP OF ME!",
    "CAN'T MOVE!",
    "DIG. DIG. DIG."
};

static const char *const digs_any_doomed[] = {
    "GOODBYE CRUEL MINE!",
    "NO MORE SHIFTS FOR ME!",
    "TELL THEM I WENT DOWN DIGGING!",
    "THIS IS FINE!"
};

static const char *const digs_any_long_absence[] = {
    "THOUGHT YOU'D GONE HOME, %T.",
    "WHERE HAVE YOU BEEN?",
    "STILL ALIVE, THEN.",
    "LONG TIME."
};

static const char *const digs_any_match_start[] = {
    "ANOTHER SHIFT.",
    "LET'S GET ON WITH IT.",
    "CLOCK'S RUNNING.",
    "DOWN WE GO."
};

static const char *const digs_any_match_end[] = {
    "THAT'S THE WHISTLE.",
    "SAME TIME TOMORROW.",
    "I'VE HAD WORSE.",
    "PACK IT IN."
};

static const char *const digs_any_idle[] = {
    "QUIET DOWN HERE.",
    "TOO QUIET.",
    "ANYBODY?",
    "JUST ME AND THE ROCK."
};

/* ---- RIVET: clipped, technical, dry ---------------------------------- */

static const char *const digs_rivet_first_meeting[] = {
    "%T. NOTED.",
    "ONE CONTACT. LOGGED.",
    "YOU'RE IN MY SURVEY.",
    "STAY OUT OF THE TUNNEL.",
    "I HAVE YOUR POSITION."
};

static const char *const digs_rivet_spotted[] = {
    "CONTACT.",
    "%T, BEARING NOTED.",
    "I SEE THE HELMET LAMP.",
    "RANGE CLOSING.",
    "THERE."
};

static const char *const digs_rivet_hurt_them[] = {
    "ON TARGET.",
    "ADJUSTING.",
    "TWO MORE OF THOSE.",
    "PREDICTABLE, %T.",
    "GOOD GROUPING."
};

static const char *const digs_rivet_hurt_by[] = {
    "ACKNOWLEDGED.",
    "INEFFICIENT, BUT IT WORKED.",
    "NOTED, %T.",
    "STRUCTURAL DAMAGE.",
    "I'LL ALLOW IT."
};

static const char *const digs_rivet_near_miss[] = {
    "WIDE BY A METRE.",
    "YOUR ELEVATION IS OFF.",
    "RECALCULATE.",
    "NO.",
    "SLOPPY, %T."
};

static const char *const digs_rivet_limb_taken[] = {
    "LOAD-BEARING, THAT.",
    "STRUCTURAL FAILURE.",
    "YOU'RE DOWN A LIMB, %T.",
    "AS DESIGNED.",
    "CLEAN CUT."
};

static const char *const digs_rivet_limb_lost[] = {
    "COMPENSATING.",
    "STILL FUNCTIONAL.",
    "THAT WAS LOAD-BEARING.",
    "MINOR SETBACK.",
    "I HAVE ANOTHER."
};

static const char *const digs_rivet_killed_them[] = {
    "SHIFT CONCLUDED.",
    "%T. FILED.",
    "AS CALCULATED.",
    "TUNNEL'S MINE.",
    "CLEAN."
};

static const char *const digs_rivet_killed_by[] = {
    "MISCALCULATION.",
    "I'LL REVISE THE SURVEY.",
    "%T. LOGGED.",
    "UNEXPECTED.",
    "THAT WAS MY ERROR."
};

static const char *const digs_rivet_revenge[] = {
    "LEDGER BALANCED.",
    "ACCOUNTS SETTLED, %T.",
    "AS PLANNED.",
    "THAT WAS OWED.",
    "CORRECTED."
};

static const char *const digs_rivet_taunted[] = {
    "IRRELEVANT.",
    "SAVE YOUR AIR.",
    "NOTED AND DISMISSED.",
    "TALK IS CHEAP DOWN HERE.",
    "MM."
};

static const char *const digs_rivet_lava_close[] = {
    "THERMAL BREACH.",
    "THE BASIN'S COMING UP.",
    "RELOCATING.",
    "THAT'S NOT SURVIVABLE.",
    "TIME TO LEAVE."
};

static const char *const digs_rivet_buried[] = {
    "ROOF FAILURE. I WARNED SOMEBODY.",
    "PINNED. WORKING ON IT.",
    "THE SPAN WAS TOO WIDE.",
    "CUTTING OUT.",
    "THIS IS WHY YOU SHORE IT UP."
};

static const char *const digs_rivet_doomed[] = {
    "SURVEY INCOMPLETE.",
    "I'D HAVE FIXED THAT TUNNEL.",
    "TELL THEM THE MATH WAS RIGHT.",
    "MY CALCULATIONS WERE SOUND!",
    "FILE IT UNDER ACCIDENT."
};

static const char *const digs_rivet_match_start[] = {
    "SURVEY BEGINS.",
    "LET'S BE EFFICIENT.",
    "I'VE READ THE SEAM.",
    "TOOLS OUT.",
    "BEGINNING WORK."
};

static const char *const digs_rivet_idle[] = {
    "GOOD ROCK HERE.",
    "MEASURING.",
    "THE SEAM RUNS EAST.",
    "NOBODY'S DIGGING PROPERLY.",
    "QUIET. I PREFER IT."
};

/* ---- CINDER: loud, boastful, direct ---------------------------------- */

static const char *const digs_cinder_first_meeting[] = {
    "FRESH MEAT!",
    "YOU! %T! COME HERE!",
    "OH GOOD, SOMEBODY TO HIT!",
    "I WAS GETTING BORED!",
    "HELLO HELLO HELLO!"
};

static const char *const digs_cinder_spotted[] = {
    "GOT YOU!",
    "RUNNING, %T? GOOD!",
    "I SEE YOU!",
    "COME HERE!",
    "THERE YOU ARE!"
};

static const char *const digs_cinder_hurt_them[] = {
    "HA!",
    "FEEL THAT, %T?",
    "MORE WHERE THAT CAME FROM!",
    "STAND STILL AND TAKE IT!",
    "THAT'S THE STUFF!"
};

static const char *const digs_cinder_hurt_by[] = {
    "IS THAT IT?",
    "TICKLES!",
    "DO IT AGAIN, %T!",
    "OW! GOOD ONE!",
    "NOW I'M AWAKE!"
};

static const char *const digs_cinder_near_miss[] = {
    "MISSED ME!",
    "TERRIBLE!",
    "MY GRANDMOTHER AIMS BETTER!",
    "AGAIN! TRY AGAIN!",
    "HA! NOTHING!"
};

static const char *const digs_cinder_limb_taken[] = {
    "THERE GOES AN ARM!",
    "YOU CAN HOP, %T!",
    "PIECES!",
    "THAT'S COMING WITH ME!",
    "HAH! LOOK AT IT GO!"
};

static const char *const digs_cinder_limb_lost[] = {
    "I DIDN'T NEED IT!",
    "TWO LEFT! PLENTY!",
    "IS THAT MINE?",
    "STILL COMING, %T!",
    "DOESN'T EVEN HURT!"
};

static const char *const digs_cinder_killed_them[] = {
    "DOWN YOU GO!",
    "THAT'S HOW IT'S DONE!",
    "SLEEP WELL, %T!",
    "WHO'S NEXT!",
    "HA HA HA!"
};

static const char *const digs_cinder_killed_by[] = {
    "NO! I HAD YOU!",
    "%T! YOU GOT LUCKY!",
    "I'LL BE BACK!",
    "THAT DOESN'T COUNT!",
    "RUBBISH!"
};

static const char *const digs_cinder_revenge[] = {
    "TOLD YOU I'D BE BACK!",
    "REMEMBER THAT, %T?",
    "NOW WE'RE EVEN!",
    "I NEVER FORGET!",
    "PAID YOU BACK!"
};

static const char *const digs_cinder_taunted[] = {
    "BIG TALK!",
    "COME SAY IT CLOSER!",
    "WORDS! ALL WORDS!",
    "I'LL SHUT YOU UP!",
    "LOUDER, %T!"
};

static const char *const digs_cinder_lava_close[] = {
    "IT'S HOT! IT'S HOT!",
    "THE FLOOR'S ON FIRE!",
    "OUT! OUT!",
    "NOT THE LAVA!",
    "MOVE YOUR LEGS!"
};

static const char *const digs_cinder_buried[] = {
    "GET IT OFF ME!",
    "THE WHOLE ROOF!",
    "I'M NOT DYING UNDER A ROCK!",
    "PUSHING! PUSHING!",
    "COWARDS! DROPPING ROCKS!"
};

static const char *const digs_cinder_doomed[] = {
    "WHAT A WAY TO GO!",
    "I'M TAKING SOMEBODY WITH ME!",
    "NO MORE SHIFTS FOR ME!",
    "WORTH IT!",
    "TELL THEM I WAS LOUD!"
};

static const char *const digs_cinder_match_start[] = {
    "LET'S GO! LET'S GO!",
    "WHO WANTS SOME!",
    "I'VE BEEN WAITING ALL DAY!",
    "HAMMER'S READY!",
    "FIRST ONE'S FREE!"
};

static const char *const digs_cinder_idle[] = {
    "BORING!",
    "SOMEBODY COME OUT!",
    "I'M NOT DIGGING, I'M HUNTING!",
    "HELLO? ANYONE?",
    "THIS IS THE WORST SHIFT."
};

/* ---- FLAMEY: mocking, playful, sideways ------------------------------ */

static const char *const digs_flamey_first_meeting[] = {
    "OOH, A NEW FRIEND.",
    "HELLO %T. LOVELY HELMET.",
    "SOMEBODY TO PLAY WITH!",
    "I WON'T BITE. PROBABLY.",
    "YOU LOOK FLAMMABLE."
};

static const char *const digs_flamey_spotted[] = {
    "PEEKABOO.",
    "I SEE A %T.",
    "THERE'S MY FAVOURITE.",
    "DON'T RUN. IT'S RUDE.",
    "HELLO AGAIN."
};

static const char *const digs_flamey_hurt_them[] = {
    "TEE HEE.",
    "DID THAT SMART, %T?",
    "OH DEAR, ARE YOU ALL RIGHT?",
    "I'M NOT EVEN TRYING.",
    "MORE? YES? MORE."
};

static const char *const digs_flamey_hurt_by[] = {
    "RUDE!",
    "I WAS BEING NICE, %T.",
    "OW. NOTED. FILED. REMEMBERED.",
    "THAT'S GOING ON YOUR RECORD.",
    "OOH, SOMEBODY'S CROSS."
};

static const char *const digs_flamey_near_miss[] = {
    "NEARLY! ALMOST! NO.",
    "SO CLOSE, %T.",
    "TRY THE OTHER END.",
    "WHEE!",
    "WAS THAT AT ME?"
};

static const char *const digs_flamey_limb_taken[] = {
    "YOU'VE DROPPED SOMETHING.",
    "IT SUITS YOU, %T. ASYMMETRY.",
    "OH, THAT'S A LOT OF RED.",
    "KEEP IT! SOUVENIR!",
    "WHOOPS."
};

static const char *const digs_flamey_limb_lost[] = {
    "THAT'S MINE, GIVE IT BACK!",
    "I LIKED THAT ONE.",
    "STILL PRETTIER THAN YOU, %T.",
    "OH, THAT'S A LOT OF ME.",
    "I'LL GROW ANOTHER."
};

static const char *const digs_flamey_killed_them[] = {
    "NIGHT NIGHT.",
    "BYE %T! LOVELY KNOWING YOU!",
    "WAS IT SOMETHING I SAID?",
    "THAT WAS FUN. WAS IT FUN?",
    "ONE DOWN, LOTS TO GO."
};

static const char *const digs_flamey_killed_by[] = {
    "WELL THAT'S NOT FAIR.",
    "%T! I'M TELLING!",
    "I'LL BE SEEING YOU.",
    "RUDE. VERY RUDE.",
    "WORTH IT FOR THE SPARKS."
};

static const char *const digs_flamey_revenge[] = {
    "I SAID I'D BE SEEING YOU.",
    "REMEMBER ME, %T?",
    "TOLD YOU I'D TELL.",
    "PATIENCE IS A VIRTUE.",
    "THAT'S THE ONE."
};

static const char *const digs_flamey_taunted[] = {
    "OOH, WORDS!",
    "SAY MORE, I LIKE IT.",
    "IS THAT YOUR BEST, %T?",
    "MMM. SPICY.",
    "KEEP GOING, IT'S CUTE."
};

static const char *const digs_flamey_lava_close[] = {
    "OH THAT'S PRETTY.",
    "IT'S ORANGE! IT'S ORANGE!",
    "TOO PRETTY. RUNNING NOW.",
    "HOT HOT HOT HOT.",
    "I LOVE FIRE BUT NOT LIKE THIS."
};

static const char *const digs_flamey_buried[] = {
    "THIS IS COSY.",
    "I'M IN THE ROCK NOW.",
    "HELP? PLEASE? ANYONE?",
    "I'VE MADE A TERRIBLE MISTAKE.",
    "SO DARK IN HERE."
};

static const char *const digs_flamey_doomed[] = {
    "GOODBYE CRUEL MINE!",
    "SAVE THE MATCHES FOR ME!",
    "AT LEAST IT'S WARM!",
    "TELL THEM I WAS FUNNY!",
    "OH THIS IS GOING TO STING."
};

static const char *const digs_flamey_match_start[] = {
    "OH GOOD, EVERYONE'S HERE.",
    "WHO WANTS A PRESENT?",
    "I'VE BROUGHT MATCHES.",
    "LET'S MAKE A MESS.",
    "PLAYTIME."
};

static const char *const digs_flamey_idle[] = {
    "LA LA LA.",
    "ANYONE WANT TO SEE A TRICK?",
    "I'VE HIDDEN SOMETHING.",
    "COME OUT, COME OUT.",
    "BORED. BORED. BORED."
};

/* ---- the miner: wry, working-class, yours ---------------------------- */

static const char *const digs_miner_first_meeting[] = {
    "COMPANY.",
    "ALL RIGHT, %T.",
    "SO IT'S YOU.",
    "GREAT. THAT ONE.",
    "HERE WE GO."
};

static const char *const digs_miner_hurt_them[] = {
    "GOT YOU.",
    "STAY DOWN, %T.",
    "THAT'S ONE.",
    "KEEP COMING.",
    "GOOD."
};

static const char *const digs_miner_hurt_by[] = {
    "RIGHT. RIGHT.",
    "THAT ALL YOU HAVE, %T?",
    "STILL HERE.",
    "NOTED.",
    "WORTH IT."
};

static const char *const digs_miner_killed_them[] = {
    "DONE.",
    "GOODNIGHT, %T.",
    "ONE LESS.",
    "THAT'S THAT.",
    "BACK TO WORK."
};

static const char *const digs_miner_killed_by[] = {
    "NOT AGAIN.",
    "%T. OF COURSE.",
    "I'M COMING BACK.",
    "RIGHT, THAT'S IT.",
    "REMEMBER THIS."
};

static const char *const digs_miner_doomed[] = {
    "GOODBYE CRUEL MINE!",
    "NO MORE SHIFTS FOR ME!",
    "SHOULD HAVE STAYED IN BED!",
    "TELL THE FOREMAN HE WAS WRONG!",
    "OH, COME ON!"
};

/* ---- the registry ---------------------------------------------------- */

typedef struct digs_line_set {
    const char *const *lines;
    vox_u16 count;
} digs_line_set;

#define DIGS_SET(array) {(array), (vox_u16)(sizeof(array) / sizeof((array)[0]))}

/*
 * Set 0 is deliberately empty and is what an unwritten cell resolves to, so
 * "nobody wrote this" is a value the index can hold rather than a special case
 * every caller has to remember to check for.
 */
static const char *const digs_set_empty[] = {""};

static const digs_line_set digs_line_sets[] = {
    {digs_set_empty, 0U},                            /*  0 */
    DIGS_SET(digs_any_first_meeting),                /*  1 */
    DIGS_SET(digs_any_spotted),                      /*  2 */
    DIGS_SET(digs_any_hurt_them),                    /*  3 */
    DIGS_SET(digs_any_hurt_by),                      /*  4 */
    DIGS_SET(digs_any_near_miss),                    /*  5 */
    DIGS_SET(digs_any_limb_taken),                   /*  6 */
    DIGS_SET(digs_any_limb_lost),                    /*  7 */
    DIGS_SET(digs_any_killed_them),                  /*  8 */
    DIGS_SET(digs_any_killed_by),                    /*  9 */
    DIGS_SET(digs_any_revenge),                      /* 10 */
    DIGS_SET(digs_any_humiliated),                   /* 11 */
    DIGS_SET(digs_any_streak),                       /* 12 */
    DIGS_SET(digs_any_saved_by),                     /* 13 */
    DIGS_SET(digs_any_teamed_up),                    /* 14 */
    DIGS_SET(digs_any_betrayed),                     /* 15 */
    DIGS_SET(digs_any_truce_offered),                /* 16 */
    DIGS_SET(digs_any_truce_accepted),               /* 17 */
    DIGS_SET(digs_any_truce_broken),                 /* 18 */
    DIGS_SET(digs_any_taunted),                      /* 19 */
    DIGS_SET(digs_any_lava_close),                   /* 20 */
    DIGS_SET(digs_any_buried),                       /* 21 */
    DIGS_SET(digs_any_doomed),                       /* 22 */
    DIGS_SET(digs_any_long_absence),                 /* 23 */
    DIGS_SET(digs_any_match_start),                  /* 24 */
    DIGS_SET(digs_any_match_end),                    /* 25 */
    DIGS_SET(digs_any_idle),                         /* 26 */

    DIGS_SET(digs_rivet_first_meeting),              /* 27 */
    DIGS_SET(digs_rivet_spotted),                    /* 28 */
    DIGS_SET(digs_rivet_hurt_them),                  /* 29 */
    DIGS_SET(digs_rivet_hurt_by),                    /* 30 */
    DIGS_SET(digs_rivet_near_miss),                  /* 31 */
    DIGS_SET(digs_rivet_limb_taken),                 /* 32 */
    DIGS_SET(digs_rivet_limb_lost),                  /* 33 */
    DIGS_SET(digs_rivet_killed_them),                /* 34 */
    DIGS_SET(digs_rivet_killed_by),                  /* 35 */
    DIGS_SET(digs_rivet_revenge),                    /* 36 */
    DIGS_SET(digs_rivet_taunted),                    /* 37 */
    DIGS_SET(digs_rivet_lava_close),                 /* 38 */
    DIGS_SET(digs_rivet_buried),                     /* 39 */
    DIGS_SET(digs_rivet_doomed),                     /* 40 */
    DIGS_SET(digs_rivet_match_start),                /* 41 */
    DIGS_SET(digs_rivet_idle),                       /* 42 */

    DIGS_SET(digs_cinder_first_meeting),             /* 43 */
    DIGS_SET(digs_cinder_spotted),                   /* 44 */
    DIGS_SET(digs_cinder_hurt_them),                 /* 45 */
    DIGS_SET(digs_cinder_hurt_by),                   /* 46 */
    DIGS_SET(digs_cinder_near_miss),                 /* 47 */
    DIGS_SET(digs_cinder_limb_taken),                /* 48 */
    DIGS_SET(digs_cinder_limb_lost),                 /* 49 */
    DIGS_SET(digs_cinder_killed_them),               /* 50 */
    DIGS_SET(digs_cinder_killed_by),                 /* 51 */
    DIGS_SET(digs_cinder_revenge),                   /* 52 */
    DIGS_SET(digs_cinder_taunted),                   /* 53 */
    DIGS_SET(digs_cinder_lava_close),                /* 54 */
    DIGS_SET(digs_cinder_buried),                    /* 55 */
    DIGS_SET(digs_cinder_doomed),                    /* 56 */
    DIGS_SET(digs_cinder_match_start),               /* 57 */
    DIGS_SET(digs_cinder_idle),                      /* 58 */

    DIGS_SET(digs_flamey_first_meeting),             /* 59 */
    DIGS_SET(digs_flamey_spotted),                   /* 60 */
    DIGS_SET(digs_flamey_hurt_them),                 /* 61 */
    DIGS_SET(digs_flamey_hurt_by),                   /* 62 */
    DIGS_SET(digs_flamey_near_miss),                 /* 63 */
    DIGS_SET(digs_flamey_limb_taken),                /* 64 */
    DIGS_SET(digs_flamey_limb_lost),                 /* 65 */
    DIGS_SET(digs_flamey_killed_them),               /* 66 */
    DIGS_SET(digs_flamey_killed_by),                 /* 67 */
    DIGS_SET(digs_flamey_revenge),                   /* 68 */
    DIGS_SET(digs_flamey_taunted),                   /* 69 */
    DIGS_SET(digs_flamey_lava_close),                /* 70 */
    DIGS_SET(digs_flamey_buried),                    /* 71 */
    DIGS_SET(digs_flamey_doomed),                    /* 72 */
    DIGS_SET(digs_flamey_match_start),               /* 73 */
    DIGS_SET(digs_flamey_idle),                      /* 74 */

    DIGS_SET(digs_miner_first_meeting),              /* 75 */
    DIGS_SET(digs_miner_hurt_them),                  /* 76 */
    DIGS_SET(digs_miner_hurt_by),                    /* 77 */
    DIGS_SET(digs_miner_killed_them),                /* 78 */
    DIGS_SET(digs_miner_killed_by),                  /* 79 */
    DIGS_SET(digs_miner_doomed)                      /* 80 */
};

#define DIGS_LINE_SET_COUNT \
    (vox_u16)(sizeof(digs_line_sets) / sizeof(digs_line_sets[0]))

/*
 * What each voice says about each stimulus.  Zero means "nothing written for
 * this voice", which resolves to the generic pool below it.
 */
static const vox_u16
digs_voice_sets[DIGS_VOICE_COUNT][VOX_DIGS_STIMULUS_COUNT] = {
    /* RIVET */
    {0U, 27U, 28U, 29U, 30U, 31U, 32U, 33U, 34U, 35U, 36U, 0U, 0U, 0U, 0U,
     0U, 0U, 0U, 0U, 37U, 38U, 39U, 40U, 0U, 41U, 0U, 42U},
    /* CINDER */
    {0U, 43U, 44U, 45U, 46U, 47U, 48U, 49U, 50U, 51U, 52U, 0U, 0U, 0U, 0U,
     0U, 0U, 0U, 0U, 53U, 54U, 55U, 56U, 0U, 57U, 0U, 58U},
    /* FLAMEY */
    {0U, 59U, 60U, 61U, 62U, 63U, 64U, 65U, 66U, 67U, 68U, 0U, 0U, 0U, 0U,
     0U, 0U, 0U, 0U, 69U, 70U, 71U, 72U, 0U, 73U, 0U, 74U},
    /* the miner */
    {0U, 75U, 0U, 76U, 77U, 0U, 0U, 0U, 78U, 79U, 0U, 0U, 0U, 0U, 0U,
     0U, 0U, 0U, 0U, 0U, 0U, 0U, 80U, 0U, 0U, 0U, 0U}
};

/* The floor: what anyone would say.  No entry here may be zero. */
static const vox_u16 digs_any_sets[VOX_DIGS_STIMULUS_COUNT] = {
    0U,     /* NONE has nothing to say, by definition */
    1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U,
    16U, 17U, 18U, 19U, 20U, 21U, 22U, 23U, 24U, 25U, 26U
};

/*
 * Tone overrides: a specific (voice, tone, stimulus) that deserves its own
 * words.  Still empty -- the fallback chain covers every cell without it, and
 * this is where the writing gets sharper rather than merely present.
 */
static const vox_u16 digs_tone_sets[DIGS_VOICE_COUNT][VOX_DIGS_TONE_COUNT]
                                   [VOX_DIGS_STIMULUS_COUNT];

static digs_line_pool digs_pool_from_set(vox_u16 set)
{
    digs_line_pool pool;
    if (set == 0U || set >= DIGS_LINE_SET_COUNT) {
        pool.first = 0U;
        pool.count = 0U;
        return pool;
    }
    pool.first = (vox_u16)(set * DIGS_LINE_SET_STRIDE);
    pool.count = digs_line_sets[set].count;
    return pool;
}

digs_line_pool digs_lines_pool(vox_u16 voice, vox_u16 tone, vox_u16 stimulus)
{
    digs_line_pool pool;
    if (stimulus >= VOX_DIGS_STIMULUS_COUNT) {
        pool.first = 0U;
        pool.count = 0U;
        return pool;
    }
    if (voice < DIGS_VOICE_COUNT && tone < VOX_DIGS_TONE_COUNT) {
        pool = digs_pool_from_set(digs_tone_sets[voice][tone][stimulus]);
        if (pool.count != 0U) {
            return pool;
        }
    }
    if (voice < DIGS_VOICE_COUNT) {
        pool = digs_pool_from_set(digs_voice_sets[voice][stimulus]);
        if (pool.count != 0U) {
            return pool;
        }
    }
    return digs_pool_from_set(digs_any_sets[stimulus]);
}

const char *digs_lines_text(vox_u16 id)
{
    vox_u16 set = (vox_u16)(id / DIGS_LINE_SET_STRIDE);
    vox_u16 index = (vox_u16)(id % DIGS_LINE_SET_STRIDE);
    if (set >= DIGS_LINE_SET_COUNT || index >= digs_line_sets[set].count) {
        return "";
    }
    return digs_line_sets[set].lines[index];
}

vox_u16 digs_lines_total(void)
{
    vox_u16 set;
    vox_u16 total = 0U;
    for (set = 0U; set < DIGS_LINE_SET_COUNT; ++set) {
        total = (vox_u16)(total + digs_line_sets[set].count);
    }
    return total;
}

vox_u16 digs_lines_voice_for(const vox_digs_match *match, vox_u16 player)
{
    if (match == 0 || !vox_digs_player_is_active(match, player) ||
        !vox_digs_player_is_bot(match, player)) {
        return (vox_u16)DIGS_VOICE_MINER;
    }
    switch (vox_digs_bot_archetype(match, player)) {
    case VOX_DIGS_ARCHETYPE_ENGINEER:
        return (vox_u16)DIGS_VOICE_RIVET;
    case VOX_DIGS_ARCHETYPE_BERSERKER:
        return (vox_u16)DIGS_VOICE_CINDER;
    default:
        break;
    }
    return (vox_u16)DIGS_VOICE_FLAMEY;
}

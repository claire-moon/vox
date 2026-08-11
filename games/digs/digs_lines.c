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
/*
 * Line ids are set number times stride plus position within the set, packed
 * into a vox_u16 because that is what the recent-line ring stores and hashes.
 *
 * The stride is therefore a hard cap on lines per set, and set count times
 * stride is a hard cap on the corpus.  It was 256, which looked generous and
 * silently broke the moment the corpus passed 256 sets: set 282 times 256
 * wrapped to set 26, so RIVET in a feud resolved to the generic idle pool and
 * read one line past the end of it.  Sixteen leaves room to deepen a pool and
 * keeps thousands of sets addressable.  digs_lines_stride_is_sound checks it.
 */
#define DIGS_LINE_SET_STRIDE 16U

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

/*
 * The expanded miner bark corpus is deliberately contextual rather than a
 * persona selector.  These are complete authored lines: no fragments are
 * assembled at runtime and none require a named target, so they remain sound
 * when a player is alone in the mine.
 */

static const char *const digs_any_move[] = {
    "KEEP MOVING.",
    "NEW TUNNEL. SAME PROBLEMS.",
    "ONE BOOT IN FRONT OF THE OTHER.",
    "MAKE ROOM."
};

static const char *const digs_any_weapon[] = {
    "TOOLS OUT.",
    "LET'S SEE WHAT THIS DOES.",
    "MAKE IT COUNT.",
    "LITTLE BIT OF PRESSURE."
};

static const char *const digs_any_miss[] = {
    "WELL, THAT WENT SOMEWHERE.",
    "THE ROCK HAD IT COMING.",
    "ADJUSTING.",
    "NOT MY BEST ANGLE."
};

static const char *const digs_any_hit[] = {
    "THAT CONNECTED.",
    "GOOD CONTACT.",
    "THERE IT IS.",
    "KEEP THE PRESSURE ON."
};

static const char *const digs_any_near_death[] = {
    "NOT TODAY.",
    "NEED A BETTER PLAN.",
    "BREATH. MOVE.",
    "TOO CLOSE."
};

static const char *const digs_any_cave_in[] = {
    "THAT ROOF WASN'T THERE A SECOND AGO.",
    "ROCK'S MOVING.",
    "MIND THE CEILING.",
    "THAT'S A LOT OF MINE."
};

static const char *const digs_any_grapple[] = {
    "ROPE OUT.",
    "HOLD FAST.",
    "UP WE GO.",
    "HOOK'S GOOD."
};

static const char *const digs_any_kill[] = {
    "ONE LESS PROBLEM.",
    "CLOCKED OUT.",
    "THAT SETTLES IT.",
    "BACK TO THE JOB."
};

static const char *const digs_any_humiliation[] = {
    "THAT ONE'S GOING IN THE LOG.",
    "SPECTACULAR.",
    "HARD TO TOP THAT.",
    "FOREMAN WOULD HAVE LOVED THAT."
};

static const char *const digs_any_escape[] = {
    "OUT. NOW.",
    "THIS WAY IS BETTER.",
    "LEAVE THE ROCK TO IT.",
    "RUN FIRST. EXPLAIN LATER."
};

static const char *const digs_miner_move[] = {
    "KEEP THE LAMP AHEAD OF THE FEET.",
    "SHIFT THE WEIGHT. KEEP GOING.",
    "THE MINE CAN CHASE ME FOR ONCE.",
    "MADE FOR A TIGHTER GAP THAN THIS.",
    "DON'T LET THE ROCK PICK THE ROUTE.",
    "A LITTLE FURTHER. ALWAYS A LITTLE FURTHER.",
    "NO TIME TO ADMIRE THE SCENERY.",
    "LEFT FOOT. RIGHT FOOT. NO ARGUMENTS."
};

static const char *const digs_miner_weapon[] = {
    "LET'S GIVE THE TOOL SOMETHING TO DO.",
    "THIS IS WHAT THEY ISSUED IT FOR.",
    "THAT'S THE GOOD HANDLE.",
    "MIND THE BACKBLAST. PROBABLY.",
    "IF IT'S LOUD, IT'S WORKING.",
    "NO DELICATE WAY TO DO THIS.",
    "JUST A SMALL PROFESSIONAL DEMONSTRATION.",
    "I PAID FOR THE WHOLE TOOL."
};

static const char *const digs_miner_miss[] = {
    "THAT WAS A WARNING SHOT. TO THE ROCK.",
    "SIGHT'S FINE. WORLD MOVED.",
    "RIGHT. LESS OF THAT.",
    "I MEANT TO OPEN A WINDOW.",
    "THE CEILING JUMPED IN FRONT OF IT.",
    "GOOD NEWS: NOBODY SAW THAT.",
    "CALL IT A TEST CUT.",
    "NEXT ONE HAS A NAME ON IT."
};

static const char *const digs_miner_hit[] = {
    "THAT'S WHERE THE AIM WAS MEANT TO LAND.",
    "SOLID. KEEP IT SOLID.",
    "THERE'S THE OPENING.",
    "I KNEW THE TOOL HAD MANNERS.",
    "THAT GOT THEIR ATTENTION.",
    "GOOD. NOW THEY'RE THINKING ABOUT IT.",
    "RIGHT ON THE SEAM.",
    "THAT ONE EARNS ITS NOISE."
};

static const char *const digs_miner_near_death[] = {
    "NOT DONE YET. NOT EVEN CLOSE.",
    "BREATHE FIRST. PANIC LATER.",
    "THAT WAS FAR TOO NEAR THE CLOCK.",
    "STILL HAVE A SHIFT TO FINISH.",
    "NO. THE MINE DOESN'T GET ME FOR FREE.",
    "LEGS WORK. USE THEM.",
    "ONE GOOD EXIT. THAT'S ALL I NEED.",
    "KEEP THE HEART INSIDE."
};

static const char *const digs_miner_cave_in[] = {
    "THAT'S NOT DUST. THAT'S A DECISION.",
    "ROOF'S CLOCKED OUT EARLY.",
    "EVERYBODY CLEAR THE SPAN.",
    "THAT TUNNEL HAS OPINIONS.",
    "MIND YOUR HELMET. MIND EVERYTHING.",
    "THE ROCK REMEMBERS WHO CUT IT.",
    "THAT'S A WHOLE WALL COMING DOWN.",
    "LET THE CEILING HAVE ITS MOMENT."
};

static const char *const digs_miner_grapple[] = {
    "HOOK'S IN. DON'T LOOK DOWN.",
    "ROPE'S GOT ME. FOR NOW.",
    "UP AND OVER, NICE AND UGLY.",
    "THAT'S A GOOD PIECE OF METAL.",
    "HANG ON. LITERALLY.",
    "THE LONG WAY IS FOR PEOPLE WITH TIME.",
    "LET THE ROPE DO THE THINKING.",
    "ONE TUG. THEN WE FLY."
};

static const char *const digs_miner_kill[] = {
    "SHIFT ENDED FOR SOMEBODY.",
    "ONE LESS LAMP IN THE DARK.",
    "THAT'S THE LAST OF THAT NOISE.",
    "CLOCK OUT. I'LL HOLD THE TUNNEL.",
    "NOTHING PERSONAL. MOSTLY.",
    "THE MINE CAN KEEP THE RECEIPT.",
    "DONE. BACK TO BREATHING.",
    "THAT'S ONE PROBLEM WITH A QUIET END."
};

static const char *const digs_miner_humiliation[] = {
    "THAT'S GOING IN THE SHIFT REPORT.",
    "SOMEBODY SAVE THAT MOMENT.",
    "I'D APOLOGISE, BUT LOOK AT IT.",
    "THE FOREMAN CAN'T TAKE THIS AWAY.",
    "THAT WAS BEAUTIFUL IN A TERRIBLE WAY.",
    "PUT A RIBBON ON THAT ONE.",
    "I'M GOING TO BE ANNOYING ABOUT THAT ALL WEEK.",
    "THAT'S WHY THEY GIVE US HELMETS. APPARENTLY."
};

static const char *const digs_miner_escape[] = {
    "OUT OF THE HOLE. NOW.",
    "LEAVE THE BRAVERY FOR THE PAYROLL.",
    "RUNNING IS A PERFECTLY GOOD PLAN.",
    "THE LAVA CAN HAVE THE TUNNEL.",
    "NOT DROWNING FOR A BET.",
    "THE EXIT'S STILL AN EXIT.",
    "KEEP THE HEAT BEHIND ME.",
    "LIVE FIRST. GLOAT LATER."
};

/* ---- tone-specific writing -------------------------------------------
 *
 * Where the mood changes what a miner would say.  These sit above the voice
 * pools in the fallback chain, so a cell that nobody wrote still resolves --
 * this layer is where the writing gets sharper rather than merely present.
 */

static const char *const digs_cinder_bonded_hurt_by[] = {
    "HA! GOOD ONE! STILL PALS!",
    "HIT ME PROPERLY, %T! DON'T BE SHY!",
    "OW! FROM YOU THAT'S ALMOST NICE!",
    "I'D RATHER IT WAS YOU THAN THEM!",
    "NO HARD FEELINGS, %T! NONE! ZERO!"
};

static const char *const digs_cinder_bonded_hurt_them[] = {
    "OW! THAT ONE HURT ME TOO!",
    "SORRY! STILL LOVE YOU! SORRY!",
    "HIT ME BACK, %T! FAIR'S FAIR!",
    "I'M NOT ENJOYING THIS! MUCH!",
    "STAY UP! I DON'T WANT YOU DOWN!"
};

static const char *const digs_cinder_bonded_killed_by[] = {
    "GOOD ONE! I'D HAVE DONE THE SAME!",
    "%T! OF ALL PEOPLE! HA!",
    "PROUD OF YOU! ANNOYED! BUT PROUD!",
    "TELL THEM IT WAS A FRIEND!",
    "NO GRUDGE! NONE! SEE YOU SOON!"
};

static const char *const digs_cinder_bonded_killed_them[] = {
    "NO! GET UP! GET UP!",
    "THAT'S THE WORST ONE I'VE DONE.",
    "I'M SORRY, %T! I'M SORRY!",
    "SOMEBODY ELSE SHOULD HAVE DONE THAT.",
    "COME BACK QUICK, %T! I'LL WAIT!"
};

static const char *const digs_cinder_bonded_saved_by[] = {
    "THAT'S MY %T!",
    "AGAIN, %T! YOU DID IT AGAIN!",
    "I KNEW YOU'D COME! I KNEW IT!",
    "NOBODY TOUCHES ME WHILE YOU'RE UP!",
    "WE GO DOWN TOGETHER OR NOT AT ALL!"
};

static const char *const digs_cinder_bonded_spotted[] = {
    "THERE'S MY FAVOURITE MINER!",
    "%T! I'VE BEEN LOOKING FOR YOU!",
    "GOOD! I FIGHT BETTER NEAR YOU!",
    "DON'T DIE TODAY, %T! THAT'S AN ORDER!",
    "COME ON! LET'S GO RUIN SOMEBODY!"
};

static const char *const digs_cinder_bonded_teamed_up[] = {
    "THAT'S US! THAT'S HOW WE DO IT!",
    "DID YOU SEE THAT, %T? DID YOU?",
    "NOBODY STANDS UP TO BOTH OF US!",
    "MY FAVOURITE NOISE! US, WORKING!",
    "LINE THEM ALL UP! WE'VE GOT TIME!"
};

static const char *const digs_cinder_bonded_truce_broken[] = {
    "FINE! BUT I STILL LIKE YOU!",
    "DEAL'S OFF, %T! WE'RE STILL US!",
    "HIT ME HARD! I'D HATE A SOFT ONE!",
    "THIS'LL HURT MY HEART, %T!",
    "BEST FIGHT OF THE DAY, THIS!"
};

static const char *const digs_cinder_feud_humiliated[] = {
    "STOP IT! STOP KILLING ME!",
    "I HATE YOU! I ACTUALLY HATE YOU!",
    "%T! THIS ISN'T FUNNY ANY MORE!",
    "ONE. I ONLY NEED ONE!",
    "I'M NOT DONE! I'M NEVER DONE!"
};

static const char *const digs_cinder_feud_hurt_by[] = {
    "OF COURSE IT'S YOU, %T!",
    "THAT'S GOING ON THE PILE, %T!",
    "YOU'LL PAY FOR EVERY ONE!",
    "I'M NOT LAUGHING NOW, AM I!",
    "HIT ME AGAIN. SEE WHAT HAPPENS."
};

static const char *const digs_cinder_feud_hurt_them[] = {
    "THAT ONE'S BEEN WAITING!",
    "I'VE OWED YOU THAT SINCE THE START!",
    "HURTS, DOESN'T IT, %T! GOOD!",
    "NOT ENOUGH! NOWHERE NEAR ENOUGH!",
    "KEEP BLEEDING, %T!"
};

static const char *const digs_cinder_feud_killed_by[] = {
    "YOU? YOU DON'T GET TO!",
    "I'M COMING BACK FOR YOU, %T!",
    "THIS CHANGES NOTHING, %T!",
    "YOU HAVEN'T FINISHED ME!",
    "AGH! NOT TO YOU! ANYONE BUT YOU!"
};

static const char *const digs_cinder_feud_killed_them[] = {
    "THAT'S WHAT YOU GET! THAT'S IT!",
    "STAY DOWN THIS TIME, %T!",
    "I'VE WANTED THAT ALL SHIFT!",
    "AND YOU STILL OWE ME, %T!",
    "NOT SORRY! NOT EVEN A LITTLE!"
};

static const char *const digs_cinder_feud_revenge[] = {
    "THAT'S FOR EVERY SINGLE ONE!",
    "STILL COUNTING, %T. STILL COUNTING!",
    "ALL OF IT! ALL AT ONCE!",
    "I DON'T FORGIVE YOU! I JUST WIN!",
    "FEELS BETTER THAN THE LAST ONE!"
};

static const char *const digs_cinder_feud_spotted[] = {
    "%T. AGAIN. GOOD.",
    "I'VE BEEN LOOKING FOR YOU, %T.",
    "THERE'S THE FACE I HATE!",
    "DON'T RUN. NOT THIS TIME.",
    "I'VE BEEN THINKING ABOUT YOU!"
};

static const char *const digs_cinder_feud_taunted[] = {
    "%T, YOU DON'T GET TO JOKE WITH ME!",
    "SHUT IT, %T. JUST SHUT IT.",
    "TALK WHILE YOU STILL HAVE TEETH!",
    "I'VE HEARD ENOUGH OUT OF YOU!",
    "EVERY WORD IS ANOTHER BRUISE!"
};

static const char *const digs_cinder_hostile_humiliated[] = {
    "RIGHT! THAT'S THREE!",
    "YOU'RE GOOD! I'M GOING TO BE BETTER!",
    "%T IS HAVING A DAY!",
    "HOW ARE YOU DOING THAT!",
    "ONE OF THESE HAS TO GO MY WAY!"
};

static const char *const digs_cinder_hostile_hurt_by[] = {
    "HARDER, %T! I MEAN IT!",
    "GOOD SWING, %T! MY TURN!",
    "BARELY FELT THE HALF OF IT!",
    "NOW YOU'VE STARTED SOMETHING!"
};

static const char *const digs_cinder_hostile_hurt_them[] = {
    "THAT'S A GOOD NOISE!",
    "SOFT! YOU'RE SOFT, %T!",
    "PLENTY MORE IN THE ARM!",
    "HALFWAY THERE, %T!",
    "STOP WRIGGLING!"
};

static const char *const digs_cinder_hostile_killed_by[] = {
    "ARGH! FINE! FINE!",
    "TAKE IT THEN! TAKE IT!",
    "WELL SWUNG, %T!",
    "THAT'S THE GAME! ARGH!"
};

static const char *const digs_cinder_hostile_killed_them[] = {
    "AND THAT'S THE JOB!",
    "GOOD FIGHT, %T! REALLY!",
    "OFF YOU GO, %T!",
    "BOOTS ON! NEXT ONE!",
    "ANOTHER ONE FOR THE PILE!"
};

static const char *const digs_cinder_hostile_revenge[] = {
    "EVEN AGAIN! LOVELY!",
    "GOT MY OWN BACK, %T!",
    "BOOKS ARE CLEAR! ON WE GO!",
    "THAT DIDN'T TAKE LONG!",
    "FAIR'S FAIR, %T! DOWN YOU WENT!"
};

static const char *const digs_cinder_hostile_spotted[] = {
    "SOMETHING TO HIT! FINALLY!",
    "STOP RIGHT THERE, %T!",
    "OI! THIS WAY!",
    "WALKING RIGHT INTO IT, %T!",
    "MY LUCKY DAY!"
};

static const char *const digs_cinder_hostile_taunted[] = {
    "SAVE IT FOR THE HAMMER!",
    "GOOD LINE! NOW DIE!",
    "YOU TALK, I SWING, %T!",
    "MOUTH LATER! FIGHT NOW!",
    "I'M NOT LISTENING! I'M RUNNING!"
};

static const char *const digs_cinder_needling_humiliated[] = {
    "ALL RIGHT, YOU'VE MADE A POINT!",
    "THIS STOPPED BEING FUNNY, %T!",
    "I WAS JOKING. YOU WEREN'T.",
    "FINE! I'M TAKING YOU SERIOUSLY!",
    "NO MORE HEAD STARTS!"
};

static const char *const digs_cinder_needling_hurt_by[] = {
    "OH, IT BITES!",
    "THERE'S A LITTLE SOMETHING!",
    "WAS THAT ON PURPOSE, %T?",
    "CUTE. VERY CUTE.",
    "PUT YOUR BACK IN NEXT TIME!"
};

static const char *const digs_cinder_needling_hurt_them[] = {
    "WAS THAT TOO HARD? SORRY!",
    "I'M USING ONE HAND, %T!",
    "STILL WARMING UP!",
    "YOU MAKE A FUNNY NOISE, %T!",
    "GO ON, HIT ME BACK. TRY."
};

static const char *const digs_cinder_needling_killed_by[] = {
    "OH, NOW YOU'RE INTERESTING!",
    "WELL LOOK AT THAT!",
    "%T HAS TEETH AFTER ALL!",
    "LUCKY! ONE FOR YOU!",
    "RIGHT. NOW I'M PAYING ATTENTION."
};

static const char *const digs_cinder_needling_killed_them[] = {
    "AND THAT'S WITH ME BEING NICE!",
    "OH DEAR, %T. WAS THAT QUICK?",
    "BETTER LUCK, %T! MUCH BETTER!",
    "YOU DID TRY. A BIT.",
    "PICK YOURSELF UP AND COME BACK!"
};

static const char *const digs_cinder_needling_revenge[] = {
    "AND WE'RE BACK TO NORMAL!",
    "ONE EACH, %T! MINE WAS BETTER!",
    "THAT WAS FOR THE CHEEK!",
    "DON'T GET IDEAS AGAIN!",
    "SEE? THAT'S THE PROPER ORDER!"
};

static const char *const digs_cinder_needling_spotted[] = {
    "OH, IT'S THE LITTLE ONE!",
    "LOOK WHO CRAWLED UP, %T!",
    "GO ON THEN. IMPRESS ME.",
    "I'LL GIVE YOU A HEAD START, %T!",
    "THAT HELMET WON'T HELP!"
};

static const char *const digs_cinder_needling_taunted[] = {
    "OOH, SOMEBODY'S FUNNY!",
    "THAT'S THE BEST YOU HAVE?",
    "NICE TRY, %T! WEAK, BUT NICE!",
    "GO ON, ONE MORE! I DARE YOU!",
    "I'VE HEARD WORSE FROM ROCKS!"
};

static const char *const digs_cinder_neutral_hurt_by[] = {
    "HARDER, %T! I FELT NOTHING!",
    "OOH! YOU PUT YOUR BACK INTO THAT!",
    "MY TURN NOW, %T!",
    "GOOD! NOW I'M ANGRY!",
    "THAT WOKE THE HAMMER UP!"
};

static const char *const digs_cinder_neutral_hurt_them[] = {
    "ONE! LET'S GO TO TWO!",
    "THAT'S THE SOUND I LIKE!",
    "YOU'RE LEAKING, %T!",
    "SOFT! YOU'RE SOFT!",
    "AGAIN! AGAIN! AGAIN!"
};

static const char *const digs_cinder_neutral_killed_by[] = {
    "ARGH! I WASN'T READY!",
    "THAT'S NOT OVER, %T!",
    "CHEAP! THAT WAS CHEAP!",
    "I WAS WINNING!",
    "SEE YOU IN A MINUTE!"
};

static const char *const digs_cinder_neutral_killed_them[] = {
    "FLAT! ABSOLUTELY FLAT!",
    "SHOULD HAVE RUN, %T!",
    "THAT'S ONE FOR THE WALL!",
    "BOOM! GOODNIGHT!",
    "I BARELY SWUNG, %T!"
};

static const char *const digs_cinder_neutral_saved_by[] = {
    "HA! YOU GOT THEM FIRST!",
    "GOOD SWING, %T! GOOD SWING!",
    "THAT ONE WAS MINE! BUT THANKS!",
    "I OWE YOU A HIT! A NICE ONE!",
    "I HAD IT HANDLED! PROBABLY!"
};

static const char *const digs_cinder_neutral_spotted[] = {
    "THERE'S A HEAD I HAVEN'T HIT YET!",
    "OI! %T! STAY RIGHT THERE!",
    "FINALLY! SOMETHING TO SWING AT!",
    "I SMELL A MINER!",
    "HAMMER UP! COMING FOR YOU, %T!"
};

static const char *const digs_cinder_neutral_teamed_up[] = {
    "TOGETHER! HIT THEM TOGETHER!",
    "HA! LOOK AT THEM RUN!",
    "GOOD! MORE OF THAT, %T!",
    "TWO HAMMERS! NO WAITING!",
    "SQUASH! LOVELY!"
};

static const char *const digs_cinder_neutral_truce_broken[] = {
    "GOOD! I HATED THE QUIET!",
    "DEAL'S DEAD! HAMMER'S UP!",
    "WAS WONDERING WHEN, %T!",
    "FINALLY! BACK TO WORK!",
    "NO MORE TALKING! SWINGING!"
};

static const char *const digs_cinder_thawing_hurt_by[] = {
    "OI! I WAS BEING NICE!",
    "IS THAT HOW WE'RE DOING IT AGAIN?",
    "FINE! ONE BACK AND WE'RE LEVEL!",
    "%T! I ALMOST LIKED YOU!",
    "CAREFUL! I WAS NEARLY DONE BEING CROSS!"
};

static const char *const digs_cinder_thawing_hurt_them[] = {
    "SORRY! HALF SORRY!",
    "THAT WAS THE OLD ME, %T!",
    "MY ARM DID THAT, %T! NOT ME!",
    "STILL A BIT ANGRY! LESS THAN BEFORE!",
    "TOOK THAT ONE OFF WHAT YOU OWE!"
};

static const char *const digs_cinder_thawing_killed_by[] = {
    "THOUGHT WE WERE PAST THAT!",
    "%T! I WAS COMING ROUND!",
    "RIGHT! BACK TO HATING YOU! MAYBE!",
    "THAT ONE HURT MORE THAN USUAL, %T!",
    "FINE! FINE! WE START AGAIN!"
};

static const char *const digs_cinder_thawing_killed_them[] = {
    "OH. THAT WENT FURTHER THAN I MEANT.",
    "GET UP! WE WEREN'T DONE TALKING!",
    "SORRY, %T! OLD HABIT!",
    "I'LL GO EASIER NEXT TIME! MAYBE!",
    "THAT WAS THE LAST OF THE GRUDGE!"
};

static const char *const digs_cinder_thawing_saved_by[] = {
    "YOU DIDN'T HAVE TO DO THAT!",
    "HUH. ALL RIGHT, %T. ALL RIGHT.",
    "THAT'S TWO GOOD THINGS NOW, %T!",
    "STOP BEING DECENT! IT'S CONFUSING!",
    "I'M STILL COUNTING! BUT THANKS!"
};

static const char *const digs_cinder_thawing_spotted[] = {
    "YOU AGAIN. I'M NOT SWINGING. YET.",
    "%T. STILL BREATHING. HUH.",
    "I'M WATCHING YOU. NOT HITTING. WATCHING.",
    "DON'T MAKE ME REGRET THIS!",
    "HANDS WHERE I CAN SEE THEM, %T!"
};

static const char *const digs_cinder_thawing_teamed_up[] = {
    "HA! THAT WORKED! DIDN'T IT WORK?",
    "AGAIN! DO THAT AGAIN, %T!",
    "SEE? BETTER THAN HITTING EACH OTHER!",
    "I GO HIGH! YOU GO LOW! GOOD!",
    "ALL RIGHT, %T. YOU CAN STAY."
};

static const char *const digs_cinder_thawing_truce_broken[] = {
    "AND I WAS WARMING UP TO YOU!",
    "BACK IN THE HOLE, %T!",
    "KNEW IT! KNEW IT! KNEW IT!",
    "SHOULD HAVE HIT YOU FIRST!",
    "RIGHT! ALL OF IT COUNTS AGAIN!"
};

static const char *const digs_cinder_truce_hurt_by[] = {
    "OI! WE SAID NO!",
    "WAS THAT YOU, %T? WAS IT?",
    "ONE MORE AND THE DEAL'S OFF!",
    "I'M NOT SWINGING BACK! LOOK AT ME NOT!",
    "ACCIDENT! SAY IT WAS AN ACCIDENT!"
};

static const char *const digs_cinder_truce_hurt_them[] = {
    "THAT WAS AN ACCIDENT! HONEST!",
    "MY HAND SLIPPED! IT SLIPPED!",
    "STILL FRIENDS, %T? STILL FRIENDS?",
    "I PULLED IT! THAT WAS A SOFT ONE!",
    "DON'T COUNT THAT! IT DOESN'T COUNT!"
};

static const char *const digs_cinder_truce_killed_by[] = {
    "WE HAD A DEAL, %T!",
    "SO THAT'S WHAT YOU ARE, %T!",
    "NO DEAL NOW! NO DEAL EVER!",
    "I KEPT MY WORD! I KEPT IT!",
    "SNAKE! ROTTEN LITTLE SNAKE!"
};

static const char *const digs_cinder_truce_killed_them[] = {
    "NO! NO! I DIDN'T MEAN IT, %T!",
    "THE ROOF DID THAT! MOSTLY THE ROOF!",
    "COME BACK AND I'LL EXPLAIN, %T!",
    "THAT'S ON ME. THAT'S ALL ON ME.",
    "I GOT EXCITED! I ALWAYS GET EXCITED!"
};

static const char *const digs_cinder_truce_saved_by[] = {
    "YOU COULD HAVE LET THEM! YOU DIDN'T!",
    "THE DEAL DIDN'T SAY THAT, %T!",
    "HA! THE TRUCE HAS TEETH!",
    "I'M KEEPING THIS DEAL FOREVER!",
    "GOOD! GREAT! THANK YOU!"
};

static const char *const digs_cinder_truce_spotted[] = {
    "STILL NOT FIGHTING! STILL BORED!",
    "%T! THE ONE I CAN'T HIT!",
    "LOOK AT US! TWO NICE MINERS!",
    "WALK PAST, %T! GOOD! WELL DONE!",
    "MY HAMMER'S DOWN! LOOK! DOWN!"
};

static const char *const digs_cinder_truce_teamed_up[] = {
    "SEE WHAT THE DEAL BUYS US!",
    "TWO OF US! POOR THEM!",
    "STAY ON MY LEFT, %T!",
    "BEST DEAL I EVER MADE, %T!",
    "MORE! FIND ME ANOTHER ONE!"
};

static const char *const digs_cinder_truce_truce_broken[] = {
    "OVER! IT'S OVER! GOOD!",
    "I WAS ITCHING ANYWAY, %T!",
    "HAMMER'S BEEN WAITING FOR THIS!",
    "NO MORE MANNERS, %T!",
    "THAT DEAL WAS BORING ANYWAY!"
};

static const char *const digs_cinder_wary_humiliated[] = {
    "EVERY TIME. IT'S ALWAYS YOU NOW.",
    "WHAT DID I DO TO YOU, %T?",
    "I DON'T EVEN SHOUT ANY MORE.",
    "YOU'VE MADE YOUR POINT. TWICE OVER.",
    "FINE. WE'RE ENEMIES. HAPPY?"
};

static const char *const digs_cinder_wary_hurt_by[] = {
    "SO THAT'S HOW IT IS, %T.",
    "YOU ACTUALLY SWUNG, %T.",
    "I HOPED YOU WOULDN'T.",
    "FINE. THAT ANSWERS THAT.",
    "I WAS STILL DECIDING!"
};

static const char *const digs_cinder_wary_hurt_them[] = {
    "YOU MADE ME DO THAT!",
    "I DIDN'T WANT TO, %T.",
    "SHOULD HAVE STAYED BACK!",
    "THAT'S ON YOU, %T. NOT ME.",
    "STOP MAKING ME."
};

static const char *const digs_cinder_wary_killed_by[] = {
    "SO YOU MEANT IT.",
    "AFTER ALL THAT? %T?",
    "I ALMOST TRUSTED YOU, %T.",
    "NO MORE FRIENDLY. NOT WITH YOU.",
    "OH. OH, THAT'S A REAL ONE."
};

static const char *const digs_cinder_wary_killed_them[] = {
    "I LIKED YOU, %T. I DID.",
    "DIDN'T ENJOY THAT ONE.",
    "WE COULD HAVE JUST DUG, %T.",
    "DON'T MAKE ME DO IT TWICE.",
    "THAT'S WHAT COMES OF IT."
};

static const char *const digs_cinder_wary_revenge[] = {
    "NO SHOUTING THIS TIME. JUST DONE.",
    "I DIDN'T WANT IT LIKE THIS, %T.",
    "THAT'S US SETTLED. SADLY.",
    "HAPPY? BECAUSE I'M NOT.",
    "THERE, %T. NOW WE'RE NOTHING."
};

static const char *const digs_cinder_wary_spotted[] = {
    "OH. %T.",
    "KEEP YOUR DISTANCE, %T.",
    "I'M WATCHING YOUR HANDS.",
    "DON'T COME ANY CLOSER.",
    "WE'RE NOT DOING THAT ANY MORE."
};

static const char *const digs_cinder_wary_taunted[] = {
    "DON'T, %T. NOT YOU.",
    "YOU USED TO MEAN IT KINDLY.",
    "SAVE THE JOKES, %T. THEY'RE SPENT.",
    "I'M NOT LAUGHING WITH YOU NOW.",
    "THAT WAS OUR JOKE. WAS."
};

static const char *const digs_flamey_bonded_hurt_by[] = {
    "OH GOOD, YOU STILL HAVE IT IN YOU.",
    "FROM YOU, %T, THAT'S ALMOST A KISS.",
    "OW! AND I DON'T EVEN MIND!",
    "GO ON. WE'RE STILL WORKING, AFTER ALL.",
    "THAT'S FAIR. IT'S THE JOB."
};

static const char *const digs_flamey_bonded_hurt_them[] = {
    "NO NO NO, I DIDN'T MEAN YOU!",
    "%T, I'M SO SORRY, STAY UP!",
    "THAT ONE HURT ME MORE. TRULY.",
    "I'D TAKE IT BACK IF I COULD.",
    "WHY WERE YOU THERE, %T. WHY."
};

static const char *const digs_flamey_bonded_killed_by[] = {
    "GOOD. I'D RATHER IT WAS YOU.",
    "%T DID IT. I'M ALMOST PROUD.",
    "NO HARD FEELINGS. NONE AT ALL.",
    "TAKE MY MATCHES. GO ON.",
    "WHAT A LOVELY WAY TO GO, %T."
};

static const char *const digs_flamey_bonded_killed_them[] = {
    "I'M SORRY. I'M SORRY. I HAD TO.",
    "COME BACK QUICK, %T. IT'S DULL ALONE.",
    "THAT WAS THE HARDEST ONE, %T.",
    "I HATED THAT. I'D DO IT AGAIN.",
    "WAIT FOR ME. I'LL BE ALONG SOON."
};

static const char *const digs_flamey_bonded_saved_by[] = {
    "AND THERE YOU WERE. ALWAYS THERE.",
    "I KNEW YOU'D COME, %T.",
    "YOU DIDN'T EVEN THINK ABOUT IT.",
    "THAT'S TWICE NOW. I'M COUNTING NICELY.",
    "SOMEBODY LOVES ME. IT'S %T."
};

static const char *const digs_flamey_bonded_spotted[] = {
    "OH GOOD, IT'S YOU. PROPERLY GOOD.",
    "%T! THE BEST BIT OF THE SHIFT.",
    "I WAS HOPING YOU WERE STILL ALIVE.",
    "DON'T DIE TODAY. THAT'S AN ORDER.",
    "MY HEART WENT UP. HOW EMBARRASSING."
};

static const char *const digs_flamey_bonded_teamed_up[] = {
    "OH, WE'RE FRIGHTENING TOGETHER.",
    "%T AND ME. THE MINE SHOULD RUN.",
    "I DIDN'T EVEN HAVE TO ASK.",
    "YOU READ MY MIND. IT'S FULL OF FIRE.",
    "BEST DAY I'VE HAD DOWN HERE."
};

static const char *const digs_flamey_bonded_truce_broken[] = {
    "OH. OH, THAT'S A REAL ONE.",
    "%T. YOU KNOW WHAT YOU'VE DONE.",
    "OF ALL THE PEOPLE. YOU.",
    "FINE. I'LL BURN SOMETHING OF YOURS."
};

static const char *const digs_flamey_feud_humiliated[] = {
    "FINE. FINE. THIS IS STILL FUN.",
    "%T, YOU ARE RUINING A LOVELY DAY.",
    "I'M SMILING. I AM SMILING.",
    "EVERY TIME. IT'S ALWAYS YOU, %T.",
    "ONE OF THESE TIMES I'M THE ONE STANDING."
};

static const char *const digs_flamey_feud_hurt_by[] = {
    "YES. YES. ADD IT TO THE PILE.",
    "YOU ALWAYS DID GO FOR THE SAME RIB.",
    "I FELT NOTHING, %T. I FELT ALL OF IT.",
    "KEEP HITTING, %T. IT KEEPS ME WARM.",
    "THAT'S NOT NEW. YOU'RE NOT NEW."
};

static const char *const digs_flamey_feud_hurt_them[] = {
    "THAT ONE HAD A DATE ON IT.",
    "I'VE PRACTISED THIS ON A ROCK, %T.",
    "DOES IT HURT IN THE OLD PLACE?",
    "I'M NOT LAUGHING. LOOK. NOT LAUGHING.",
    "SIX MORE OF THOSE AND WE'RE STARTING."
};

static const char *const digs_flamey_feud_killed_by[] = {
    "GOOD. NOW YOU HAVE TO DO IT AGAIN.",
    "I'LL BE ALONG, %T. I ALWAYS AM.",
    "YOU STILL DON'T GET TO FINISH IT.",
    "COUNT IT, %T. I DO.",
    "SEE YOU IN A MINUTE. SMILING."
};

static const char *const digs_flamey_feud_killed_them[] = {
    "THERE. THAT'S THE ONE I WANTED.",
    "I'VE BEEN CARRYING THAT ALL SHIFT.",
    "GOODBYE %T. I MEANT ALL OF IT.",
    "NO JOKE. JUST THIS.",
    "THE FIRE WAS ALWAYS FOR YOU."
};

static const char *const digs_flamey_feud_revenge[] = {
    "THERE IT IS. I TOLD THE ROCKS I WOULD.",
    "PAID, %T. WITH INTEREST AND MATCHES.",
    "I DIDN'T ENJOY THAT. LIAR. I DID.",
    "THAT WAS SITTING IN MY POCKET.",
    "WE START AGAIN AT NOTHING, %T."
};

static const char *const digs_flamey_feud_spotted[] = {
    "THERE YOU ARE. I'VE BEEN COUNTING.",
    "%T. STILL BREATHING. WE'LL FIX THAT.",
    "I KEPT A LIST, %T. YOU'RE ALL OF IT.",
    "NO GAMES TODAY. WELL. ONE GAME.",
    "I'D KNOW THAT WALK ANYWHERE."
};

static const char *const digs_flamey_feud_taunted[] = {
    "SAY IT. I'LL KEEP IT WITH THE OTHERS.",
    "I STOPPED FINDING YOU FUNNY, %T.",
    "TALK. I'M BUSY WITH THE FUSE.",
    "YOU USED TO BE MORE ORIGINAL, %T.",
    "MM. NO. NOT THIS TIME."
};

static const char *const digs_flamey_hostile_humiliated[] = {
    "THIS IS BECOMING A HABIT, %T.",
    "RIGHT. NOW IT'S GETTING PERSONAL, %T.",
    "STILL FUNNY. LESS FUNNY. STILL FUNNY.",
    "YOU'RE COSTING ME MY GOOD MOOD.",
    "I'LL START TRYING SOON. HONESTLY."
};

static const char *const digs_flamey_hostile_hurt_by[] = {
    "OW. WELL. THAT'S THE JOB.",
    "FAIR ENOUGH. STILL KILLING YOU.",
    "YOU'RE WORKING HARDER THAN I AM, %T.",
    "OOH. A LITTLE ONE.",
    "THAT WAS ALLOWED. THE NEXT ISN'T."
};

static const char *const digs_flamey_hostile_hurt_them[] = {
    "THAT'S ONE. I DO THEM IN THREES.",
    "HOLD THERE, %T. NEARLY TIDY.",
    "%T BLEEDS THE ORDINARY COLOUR.",
    "NO HARD FEELINGS. SOME HARD ROCKS.",
    "GOOD. THE NEXT BIT IS THE FUN BIT."
};

static const char *const digs_flamey_hostile_killed_by[] = {
    "OH. WELL PLAYED, I SUPPOSE.",
    "THAT'S THE JOB BOTH WAYS.",
    "SEE YOU IN THE NEXT ONE, %T.",
    "NO HARM DONE. LOTS OF HARM DONE.",
    "I WASN'T FINISHED, BUT ALL RIGHT."
};

static const char *const digs_flamey_hostile_killed_them[] = {
    "THAT'S %T TIDIED AWAY.",
    "THANK YOU FOR YOUR TIME, %T.",
    "OFF THE FLOOR AND ONTO THE FIRE.",
    "NOT SPITE. JUST HOUSEKEEPING.",
    "NEXT ONE. WHERE'S THE NEXT ONE."
};

static const char *const digs_flamey_hostile_revenge[] = {
    "BOOKS CLOSED. NEW BOOKS OPEN.",
    "YOU HAD THAT COMING BY ARITHMETIC.",
    "EVEN, %T. NOW WATCH ME GET AHEAD.",
    "TOOK IT BACK. I ALWAYS DO.",
    "ONE FOR ONE. LOVELY MATHS."
};

static const char *const digs_flamey_hostile_spotted[] = {
    "OH GOOD. SOMETHING TO SET ALIGHT.",
    "STAND STILL, %T. IT'S QUICKER.",
    "NOTHING PERSONAL. I DO THIS TO EVERYONE.",
    "THERE'S A JOB HERE. HELLO, JOB.",
    "DON'T TAKE IT BADLY, %T. TAKE IT FAST."
};

static const char *const digs_flamey_hostile_taunted[] = {
    "MMM. SAVE IT FOR THE ROCKS.",
    "TALK IF IT HELPS YOU, %T.",
    "I'M NOT LISTENING. I'M MEASURING.",
    "WORDS DON'T BURN. YOU DO.",
    "GO ON THEN, %T. I'VE GOT AGES."
};

static const char *const digs_flamey_needling_humiliated[] = {
    "OH THIS IS A BIT SILLY NOW.",
    "%T, YOU'RE SPOILING THE JOKE.",
    "HOW MANY IS THAT? DON'T ANSWER.",
    "I'M LETTING YOU. THAT'S MY STORY.",
    "RIGHT, I'M GOING TO START CHEATING."
};

static const char *const digs_flamey_needling_hurt_by[] = {
    "OOH! CHEEKY.",
    "SOMEBODY'S BEEN PRACTISING.",
    "THAT ALMOST TICKLED, %T.",
    "FINE. FINE. NOW I'M INTERESTED.",
    "YOU'VE WOKEN THE FUNNY BONE."
};

static const char *const digs_flamey_needling_hurt_them[] = {
    "OOP! DID I DO THAT?",
    "YOU MAKE A LOVELY NOISE, %T.",
    "THAT WAS A TEST, %T. YOU FAILED.",
    "AGAIN? YES? YOU SAID YES.",
    "I'M KEEPING SCORE BADLY ON PURPOSE."
};

static const char *const digs_flamey_needling_killed_by[] = {
    "HA! ALL RIGHT, THAT WAS GOOD.",
    "OW. OW. STILL FUN THOUGH.",
    "LUCKY, %T. LUCKY LUCKY LUCKY.",
    "I'LL WANT THAT ONE BACK LATER.",
    "YOU'VE STARTED SOMETHING SILLY, %T."
};

static const char *const digs_flamey_needling_killed_them[] = {
    "OH, YOU WEREN'T READY. MY MISTAKE.",
    "TAG. YOU'RE DEAD, %T.",
    "THAT'S THE GAME. GOOD GAME. BAD GAME.",
    "WELL THAT ESCALATED NICELY.",
    "DO YOU WANT ANOTHER GO?"
};

static const char *const digs_flamey_needling_revenge[] = {
    "TAG BACK! MY TURN! MY TURN!",
    "SEE, %T? I CAN DO IT TOO.",
    "THAT'S EVEN. SHALL WE GO ODD?",
    "I WAITED THE WHOLE TWO MINUTES.",
    "YOU STARTED IT, %T. I FINISHED IT."
};

static const char *const digs_flamey_needling_spotted[] = {
    "OOH, IT'S YOU. DO SOMETHING SILLY.",
    "HELLO %T. LOST YOUR PICK AGAIN?",
    "I'VE LEFT A PRESENT IN THAT TUNNEL.",
    "DON'T LOOK DOWN. NO REASON.",
    "GUESS WHICH HAND HAS THE MATCH, %T."
};

static const char *const digs_flamey_needling_taunted[] = {
    "OOH, A SPEECH! DO THE ENDING AGAIN.",
    "THAT'S NEARLY A JOKE, %T.",
    "I'M WRITING THAT ON THE WALL.",
    "MORE, %T! I'M COLLECTING THEM!",
    "YOU TALK LIKE A FALLING ROCK."
};

static const char *const digs_flamey_neutral_hurt_by[] = {
    "OW! DO IT SOFTER.",
    "YOU'VE WOKEN SOMETHING UP, %T.",
    "I FELT THAT IN MY TEETH.",
    "OOH. UNKIND, %T. LOVELY, BUT UNKIND.",
    "I'M ADDING THAT TO THE LIST."
};

static const char *const digs_flamey_neutral_hurt_them[] = {
    "SPLASH!",
    "THAT'S ONE FOR THE SCRAPBOOK.",
    "DOES IT WHISTLE WHEN YOU BREATHE, %T?",
    "OH %T, YOU'RE LEAKING.",
    "I'LL DO THE OTHER SIDE TO MATCH."
};

static const char *const digs_flamey_neutral_killed_by[] = {
    "OW. WELL. THAT'S THAT THEN.",
    "SEE YOU IN A MINUTE, %T.",
    "PUT ME SOMEWHERE WARM.",
    "HA! DIDN'T EVEN HURT. IT DID.",
    "LOVELY. I'LL HAVE ANOTHER GO, %T."
};

static const char *const digs_flamey_neutral_killed_them[] = {
    "OOPS, ALL DEAD.",
    "SLEEP TIGHT, %T. MIND THE LAVA.",
    "THAT'S A LOVELY SHAPE YOU'VE MADE.",
    "I'LL TELL THEM YOU TRIED, %T.",
    "DID ANYONE ELSE SEE THAT? NO? SHAME."
};

static const char *const digs_flamey_neutral_saved_by[] = {
    "OH! A GIFT! I LIKE GIFTS.",
    "WHY DID YOU DO THAT, %T?",
    "THAT WAS ALMOST NICE OF YOU.",
    "I HAD IT HANDLED, %T. MOSTLY. NO.",
    "CURIOUS. VERY CURIOUS."
};

static const char *const digs_flamey_neutral_spotted[] = {
    "OH GOOD, A MOVING TARGET.",
    "SOMETHING'S TWITCHING OVER THERE.",
    "I'VE FOUND A %T. CAN I KEEP IT?",
    "STAY EXACTLY THERE, %T. FOREVER.",
    "GUESS WHO."
};

static const char *const digs_flamey_neutral_teamed_up[] = {
    "OOH, WE'RE DOING THIS TOGETHER.",
    "YOU HOLD THEM, %T. I'LL DECORATE.",
    "TWO IS SUCH A NICE NUMBER.",
    "THAT WAS ALMOST TIDY.",
    "SAME AGAIN? YES? SAME AGAIN."
};

static const char *const digs_flamey_neutral_truce_broken[] = {
    "THAT DIDN'T LAST. THEY NEVER DO.",
    "BACK TO THE FUN BIT.",
    "PROMISES ARE SO FLAMMABLE.",
    "THE DEAL'S OFF, %T. LOVELY.",
    "PEACE WAS BORING ANYWAY."
};

static const char *const digs_flamey_thawing_hurt_by[] = {
    "AND WE WERE GETTING ON SO WELL.",
    "IS THIS US GOING BACKWARDS, %T?",
    "OW. I'LL CALL THAT AN ACCIDENT.",
    "I WAS BEING SO WELL BEHAVED.",
    "HMM. ONE MORE AND I STOP LIKING YOU."
};

static const char *const digs_flamey_thawing_hurt_them[] = {
    "SORRY! WELL. HALF SORRY.",
    "THAT WAS SMALLER THAN USUAL, %T.",
    "I PULLED IT. DON'T TELL ANYONE.",
    "SEE? I CAN BE GENTLE. THAT WAS GENTLE.",
    "OLD HABITS, %T. VERY OLD."
};

static const char *const digs_flamey_thawing_killed_by[] = {
    "AND I WAS WARMING TO YOU.",
    "RIGHT BACK TO THE START, %T.",
    "OH, THAT STINGS IN A NEW WAY.",
    "I'LL FORGIVE IT. EVENTUALLY. LOUDLY.",
    "WELL. THAT ANSWERS THAT, %T."
};

static const char *const digs_flamey_thawing_killed_them[] = {
    "OH. I DIDN'T MEAN ALL OF THAT.",
    "SORRY %T. THE HABIT WON.",
    "WE WERE ALMOST FRIENDS. ALMOST.",
    "COME BACK. I'LL BE NICER. MAYBE.",
    "THAT'S SET US BACK A BIT, %T."
};

static const char *const digs_flamey_thawing_saved_by[] = {
    "OH. OH THAT WAS ACTUALLY KIND.",
    "%T, WHAT ARE YOU DOING TO ME?",
    "I DON'T KNOW WHERE TO PUT THAT.",
    "FINE. FINE! I LIKE YOU A LITTLE.",
    "YOU'RE MAKING THIS DIFFICULT, %T."
};

static const char *const digs_flamey_thawing_spotted[] = {
    "OH. IT'S YOU. THAT'S FINE.",
    "I'M NOT PLEASED TO SEE YOU, %T.",
    "STILL BREATHING. GOOD. NO REASON.",
    "DON'T MAKE ME LIKE YOU.",
    "I HAVEN'T DECIDED ABOUT YOU YET, %T."
};

static const char *const digs_flamey_thawing_teamed_up[] = {
    "LOOK AT US. LOOK AT US!",
    "THAT WAS ALMOST A FRIENDSHIP, %T.",
    "I COULD GET USED TO THIS. DON'T.",
    "WE'RE NOT A PAIR. WE JUST MATCH.",
    "AGAIN, AND I'LL START TRUSTING YOU."
};

static const char *const digs_flamey_thawing_truce_broken[] = {
    "BACK IN THE BAD BOOKS, %T.",
    "I NEARLY LIKED YOU. NEARLY.",
    "SHAME. THAT WAS GOING SOMEWHERE.",
    "RIGHT. THE OLD ME, THEN."
};

static const char *const digs_flamey_truce_hurt_by[] = {
    "WAS THAT ON PURPOSE? SAY NO.",
    "%T. WE HAD SOMETHING LOVELY.",
    "I'M COUNTING. I'M ONLY COUNTING.",
    "ONE MORE, %T, AND THE DEAL BURNS.",
    "OW! ACCIDENT! THAT WAS AN ACCIDENT."
};

static const char *const digs_flamey_truce_hurt_them[] = {
    "THAT WASN'T ME! THAT WAS THE ROCK.",
    "OOPS. STILL FRIENDS, %T?",
    "SORRY! HAND SLIPPED. HONESTLY.",
    "PRETEND THAT DIDN'T HAPPEN, %T.",
    "THAT WAS THE OLD ARRANGEMENT TALKING."
};

static const char *const digs_flamey_truce_killed_by[] = {
    "WE SHOOK ON IT. WE SHOOK!",
    "OH %T. AND I WAS BEHAVING.",
    "THAT'S CHEATING. DELICIOUS CHEATING.",
    "I'M DISAPPOINTED, %T. AND IMPRESSED.",
    "NOTED FOR NEXT TIME. THERE IS ONE."
};

static const char *const digs_flamey_truce_killed_them[] = {
    "OH NO. OH NO NO NO.",
    "THAT'S NOT WHAT WE AGREED, IS IT.",
    "IN MY DEFENCE, YOU WERE STANDING THERE.",
    "SORRY %T! IT JUST HAPPENED!",
    "NOBODY SAW. NOBODY SAW."
};

static const char *const digs_flamey_truce_saved_by[] = {
    "YOU KEPT YOUR WORD. HOW STRANGE.",
    "THE DEAL HOLDS, %T. LOOK AT THAT.",
    "OH, IT'S A REAL ONE. THE TRUCE.",
    "I'D HUG YOU BUT YOU'D FLINCH.",
    "GOOD. NOW I OWE YOU. HORRIBLE."
};

static const char *const digs_flamey_truce_spotted[] = {
    "LOOK AT ME NOT SETTING A TRAP.",
    "HELLO %T. HANDS WHERE I CAN SEE.",
    "WALK ON BY. I'M BEING GOOD.",
    "OOH, TEMPTING. NO. NO!",
    "I'M SMILING. IT'S A FRIENDLY ONE."
};

static const char *const digs_flamey_truce_teamed_up[] = {
    "SEE? WE'RE BETTER POINTED OUTWARD.",
    "THE DEAL PAYS FOR ITSELF, %T.",
    "YOU DO THE HOLES, I'LL DO THE FIRE.",
    "SOMEBODY ELSE IS THE TARGET, %T.",
    "BEST DEAL I HAVE EVER MADE."
};

static const char *const digs_flamey_truce_truce_broken[] = {
    "OH GOOD. I WAS ITCHING.",
    "THE DEAL'S ASH NOW, %T.",
    "I ONLY KEPT IT TO SEE YOU BREAK IT.",
    "SUCH A SHORT LITTLE PEACE. LOVELY.",
    "NOW WE CAN BOTH STOP PRETENDING."
};

static const char *const digs_flamey_wary_humiliated[] = {
    "YOU'RE ENJOYING THIS. YOU ARE.",
    "I KEEP GIVING YOU THE BENEFIT, %T.",
    "ALL RIGHT. THE SMILE'S GONE NOW.",
    "THAT'S ENOUGH OF THAT. TRULY.",
    "I WAS SO NICE TO YOU, %T. ONCE."
};

static const char *const digs_flamey_wary_hurt_by[] = {
    "SO THAT'S HOW IT IS NOW.",
    "YOU AIMED, %T. YOU ACTUALLY AIMED.",
    "OH. OH, THAT'S DISAPPOINTING.",
    "RIGHT. NO MORE PRESENTS FOR YOU.",
    "I'D HAVE SHARED MY MATCHES, %T."
};

static const char *const digs_flamey_wary_hurt_them[] = {
    "SORRY. NOT SORRY. A BIT SORRY.",
    "YOU MADE ME DO THAT ONE, %T.",
    "I DIDN'T ENJOY IT AS MUCH AS USUAL.",
    "THAT'S FAR ENOUGH. STOP THERE.",
    "I DON'T WANT TO. I WILL THOUGH."
};

static const char *const digs_flamey_wary_killed_by[] = {
    "YOU REALLY DID IT. HUH.",
    "AND I WAS BEING SO GOOD.",
    "NOT FROM YOU, %T. ANYONE BUT.",
    "OH. THAT ONE STINGS DIFFERENTLY.",
    "I'LL REMEMBER YOU SMILED, %T."
};

static const char *const digs_flamey_wary_killed_them[] = {
    "I DIDN'T WANT THAT ONE.",
    "GOODBYE, %T. THAT'S A SHAME.",
    "THERE. NOBODY LAUGH. NOT EVEN ME.",
    "YOU SHOULD HAVE STAYED KIND, %T.",
    "THAT WAS THE QUIET WAY. YOU'RE WELCOME."
};

static const char *const digs_flamey_wary_revenge[] = {
    "THERE. I DIDN'T LIKE DOING THAT.",
    "WE'RE LEVEL, %T. IT'S NOT NICE UP HERE.",
    "THAT'S THE LAST WARM ONE.",
    "I HOPED I'D MISS. I DIDN'T.",
    "NOW WE BOTH KNOW WHERE WE ARE."
};

static const char *const digs_flamey_wary_spotted[] = {
    "OH. IT'S YOU. HELLO. HELLO.",
    "I'M KEEPING THE WALL BETWEEN US, %T.",
    "YOU USED TO WAVE. YOU'RE NOT WAVING.",
    "STAY WHERE I CAN SEE THE LAMP.",
    "I'M SMILING FROM OVER HERE, %T."
};

static const char *const digs_flamey_wary_taunted[] = {
    "YOU NEVER USED TO TALK LIKE THAT.",
    "IS THAT REALLY YOU, %T?",
    "MM. THAT'S A NEW VOICE ON YOU.",
    "SAY SOMETHING FUNNY AND I'LL BELIEVE IT.",
    "I'M NOT PLAYING THAT ONE ANY MORE."
};

static const char *const digs_miner_bonded_hurt_by[] = {
    "FAIR. YOU HAD TO.",
    "YOU'RE ALLOWED, %T.",
    "GOOD HIT. HATED IT.",
    "IF IT'S ANYONE, I'D RATHER IT'S YOU.",
    "I'D HAVE DONE THE SAME."
};

static const char *const digs_miner_bonded_hurt_them[] = {
    "SORRY. GENUINELY SORRY.",
    "THEY PAY US TO DO THIS, %T.",
    "I HATE THIS PART, %T.",
    "STAY DOWN AND I'LL STOP.",
    "YOU KNOW I DON'T MEAN IT."
};

static const char *const digs_miner_bonded_killed_by[] = {
    "GOOD. GLAD IT WAS YOU.",
    "GO ON, %T. WIN IT.",
    "NO HARD FEELINGS. I MEAN IT.",
    "TAKE MY PICK. YOU'LL NEED IT.",
    "TELL THEM WE WERE FRIENDS."
};

static const char *const digs_miner_bonded_killed_them[] = {
    "SORRY, MATE. THE JOB.",
    "I'LL BUY YOU ONE UPSTAIRS, %T.",
    "WISH IT HAD BEEN SOMEONE ELSE.",
    "YOU'D HAVE DONE IT TO ME.",
    "REST, %T. YOU'VE EARNED IT."
};

static const char *const digs_miner_bonded_saved_by[] = {
    "THAT'S TWICE NOW.",
    "I'D BE COOLING ON THE FLOOR, %T.",
    "YOU ALWAYS TURN UP, %T.",
    "ONE DAY I'LL GET THERE FIRST.",
    "SAME AS EVER. THANK YOU."
};

static const char *const digs_miner_bonded_spotted[] = {
    "THERE'S A FACE I DON'T MIND.",
    "STILL BREATHING, %T. GOOD.",
    "BEST NEWS OF THE SHIFT.",
    "COME ON, THEN. STICK CLOSE.",
    "I WAS HOPING IT WAS YOU, %T."
};

static const char *const digs_miner_bonded_teamed_up[] = {
    "LIKE WE'VE DONE IT FOR YEARS.",
    "%T AND ME. GOD HELP THEM.",
    "DIDN'T EVEN HAVE TO LOOK.",
    "BEST CREW I'VE HAD.",
    "ONE MORE AND IT'S JUST US TWO."
};

static const char *const digs_miner_bonded_truce_broken[] = {
    "TELL ME YOU HAD A REASON, %T.",
    "AFTER EVERYTHING, %T.",
    "I DIDN'T THINK YOU HAD IT IN YOU.",
    "THAT ONE ACTUALLY HURT.",
    "FINE. SHOW ME WHAT WE ARE NOW."
};

static const char *const digs_miner_feud_humiliated[] = {
    "I'M NOT LEARNING ANYTHING FROM THIS.",
    "KEEP COUNTING, %T. I AM.",
    "ONE OF THESE SHIFTS IT'LL BE YOU, %T.",
    "THIS ONLY MAKES IT WORSE FOR YOU.",
    "I'VE GOT NOTHING BUT TIME DOWN HERE."
};

static const char *const digs_miner_feud_hurt_by[] = {
    "COURSE IT WAS YOU, %T.",
    "ADD IT TO THE LIST.",
    "YOU'RE ONLY MAKING IT LONGER, %T.",
    "I'VE FELT WORSE FROM YOU BEFORE.",
    "GOOD. NOW I'M NOT HOLDING BACK."
};

static const char *const digs_miner_feud_hurt_them[] = {
    "FIRST INSTALMENT.",
    "THAT'S NOT EVEN THE INTEREST.",
    "YOU OWE ME A LOT MORE THAN THAT.",
    "BLEED A WHILE, %T.",
    "I'VE WANTED THAT SINCE THE WHISTLE."
};

static const char *const digs_miner_feud_killed_by[] = {
    "YOU. ALWAYS YOU.",
    "THIS CHANGES NOTHING, %T.",
    "I'LL BE ALONG SHORTLY.",
    "ENJOY THE NEXT TEN MINUTES.",
    "I'M NOT FINISHED WITH YOU, %T."
};

static const char *const digs_miner_feud_killed_them[] = {
    "THAT'S BEEN A LONG TIME COMING.",
    "STAY IN THE DIRT.",
    "I'VE HAD THAT ONE PLANNED, %T.",
    "NOT SORRY. NOT EVEN A LITTLE.",
    "THE MINE CAN HAVE YOU NOW."
};

static const char *const digs_miner_feud_revenge[] = {
    "THAT'S FOR THE LAST TIME AND THE ONE BEFORE.",
    "THE BOOK'S STILL OPEN, %T.",
    "DON'T THINK THAT MAKES US EVEN, %T.",
    "ONE OFF THE TALLY.",
    "I TOLD YOU I'D REMEMBER."
};

static const char *const digs_miner_feud_spotted[] = {
    "THERE'S THAT FACE.",
    "I'VE BEEN WAITING ALL SHIFT FOR YOU.",
    "%T. STILL BREATHING. WE'LL FIX THAT.",
    "NO RUNNING THIS TIME.",
    "I KNOW WHAT YOU DID, %T."
};

static const char *const digs_miner_feud_taunted[] = {
    "SAVE IT FOR THE SURFACE.",
    "YOU TALK MORE EVERY TIME YOU LOSE.",
    "TALK'S ALL YOU'VE GOT LEFT, %T.",
    "I STOPPED LISTENING SHIFTS AGO.",
    "NOTHING YOU SAY GETS YOU OUT OF THIS."
};

static const char *const digs_miner_hostile_humiliated[] = {
    "YOU'RE HAVING A BETTER DAY THAN ME.",
    "I'VE LOST COUNT NOW, %T.",
    "THIS IS BECOMING MY WHOLE SHIFT.",
    "YOU'RE GOOD AT THIS, %T. FINE.",
    "SOMETHING'S GOT TO CHANGE."
};

static const char *const digs_miner_hostile_hurt_by[] = {
    "FAIR ENOUGH.",
    "YOU'RE WORKING TOO, THEN.",
    "I'VE HAD WORSE OFF A ROCKFALL, %T.",
    "DOESN'T CHANGE THE JOB, %T.",
    "THAT'LL BRUISE."
};

static const char *const digs_miner_hostile_hurt_them[] = {
    "PROGRESS.",
    "HALFWAY THERE.",
    "THAT'S THE JOB.",
    "STILL ON THE CLOCK, %T.",
    "EASIER IF YOU DON'T STRUGGLE."
};

static const char *const digs_miner_hostile_killed_by[] = {
    "YOU EARNED THAT.",
    "GOOD DAY'S WORK, %T.",
    "I'LL BE UP AGAIN.",
    "SAME JOB, OTHER END OF IT.",
    "NO COMPLAINTS, %T."
};

static const char *const digs_miner_hostile_killed_them[] = {
    "THAT'S YOU SEEN TO.",
    "CLOCK OFF, %T.",
    "NO HARD FEELINGS.",
    "ON TO THE NEXT ONE."
};

static const char *const digs_miner_hostile_revenge[] = {
    "THAT SQUARES IT.",
    "YOU GOT ME, I GOT YOU.",
    "NO GRUDGE. JUST TIDYING UP.",
    "BACK TO NIL, %T.",
    "THAT ONE WAS OWING."
};

static const char *const digs_miner_hostile_spotted[] = {
    "WORK TO DO.",
    "THAT'S THE ONE I CAME FOR.",
    "%T. NOTHING PERSONAL.",
    "STAND STILL, %T. QUICKER THAT WAY.",
    "ANOTHER JOB."
};

static const char *const digs_miner_hostile_taunted[] = {
    "IF YOU SAY SO.",
    "WORDS DON'T DIG.",
    "YOU'LL WANT THAT BREATH LATER, %T.",
    "RIGHT. GET ON WITH IT.",
    "I'M NOT PAID TO ARGUE."
};

static const char *const digs_miner_needling_humiliated[] = {
    "ALL RIGHT, THE JOKE'S ON ME.",
    "I WALKED INTO THAT. TWICE.",
    "YOU'RE ENJOYING THIS, %T.",
    "STARTING TO TAKE THIS PERSONALLY.",
    "SOMEBODY'S HAVING A LAUGH."
};

static const char *const digs_miner_needling_hurt_by[] = {
    "OH, YOU'RE TRYING NOW.",
    "SOMEBODY'S BEEN PRACTISING.",
    "SLOW LEARNER, BUT A LEARNER.",
    "GO ON THEN, %T. AGAIN.",
    "NOT EVEN A DENT."
};

static const char *const digs_miner_needling_hurt_them[] = {
    "DIDN'T EVEN AIM.",
    "YOU'RE EASIER THAN THE ROCK, %T.",
    "STILL WANT A GO, %T?",
    "THAT'S YOUR HELMET RATTLING.",
    "I CAN DO THIS ALL SHIFT."
};

static const char *const digs_miner_needling_killed_by[] = {
    "OH, NOW YOU'RE SERIOUS.",
    "LOOK AT YOU, %T.",
    "ALL RIGHT, THAT WAS A GOOD ONE.",
    "I ASKED FOR THAT, DIDN'T I.",
    "DON'T LET IT GO TO YOUR HEAD, %T."
};

static const char *const digs_miner_needling_killed_them[] = {
    "WELL. THAT WAS SHORT.",
    "AND YOU TALKED SO MUCH, %T.",
    "BETTER LUCK NEXT SHIFT, %T.",
    "I'VE HAD HARDER SEAMS.",
    "THAT'S THE TESTING DONE."
};

static const char *const digs_miner_needling_revenge[] = {
    "SHORT REIGN, %T.",
    "TOLD YOU NOT TO ENJOY IT.",
    "BACK DOWN YOU GO.",
    "THAT'S THE JOKE FINISHED."
};

static const char *const digs_miner_needling_spotted[] = {
    "OH, IT'S THE EXPERT.",
    "STILL LOST, %T?",
    "NICE OF YOU TO WANDER OVER.",
    "I WONDERED WHERE THAT SMELL WAS.",
    "LET'S SEE WHAT YOU'RE MADE OF, %T."
};

static const char *const digs_miner_needling_taunted[] = {
    "GOOD ONE. WRITE IT DOWN.",
    "YOU REHEARSE THAT, %T?",
    "THE ROCK'S BETTER COMPANY.",
    "GOT ANY MORE OF THOSE?",
    "I'LL LAUGH LATER."
};

static const char *const digs_miner_neutral_hurt_by[] = {
    "FINE. ADD IT TO THE LIST.",
    "HOPE THAT WAS WORTH THE SWING.",
    "I'VE BEEN HIT BY ROCKS, %T.",
    "BRUISES HEAL. MOSTLY.",
    "COULD HAVE ASKED FIRST."
};

static const char *const digs_miner_neutral_hurt_them[] = {
    "STILL UPRIGHT? GIVE IT A MINUTE.",
    "THAT'S THE JOB, %T.",
    "PAPERWORK LATER.",
    "ANOTHER ONE OF THOSE SHOULD DO IT.",
    "YOU'LL FEEL THAT TOMORROW, %T."
};

static const char *const digs_miner_neutral_killed_by[] = {
    "WELL. THAT'S THE DAY GONE.",
    "%T, WAS IT. FINE.",
    "SHOULD HAVE TAKEN THE OTHER TUNNEL.",
    "NO PENSION EITHER.",
    "SEE YOU IN A MINUTE."
};

static const char *const digs_miner_neutral_killed_them[] = {
    "TIDY.",
    "CLOCK OUT, %T.",
    "NO HARD FEELINGS, %T. NONE AT ALL.",
    "SOMEBODY HAS TO SWEEP UP.",
    "RIGHT. WHERE WAS I."
};

static const char *const digs_miner_neutral_saved_by[] = {
    "DIDN'T ASK. STILL COUNTS.",
    "THAT WAS CONVENIENT, %T.",
    "ALL RIGHT, %T. THAT'S ONE FOR YOU.",
    "SUPPOSE I SHOULD SAY SOMETHING.",
    "YOU DIDN'T HAVE TO DO THAT."
};

static const char *const digs_miner_neutral_spotted[] = {
    "ANOTHER BODY IN THE WAY.",
    "%T. COULD BE WORSE.",
    "SEEN YOU, %T. WISH I HADN'T.",
    "THAT'S MY SEAM YOU'RE STANDING ON.",
    "OVERTIME, THEN."
};

static const char *const digs_miner_neutral_teamed_up[] = {
    "TWO PAIRS OF HANDS. FINE.",
    "DON'T GET USED TO IT, %T.",
    "WORKED, DIDN'T IT.",
    "SAME AGAIN AND WE'RE DONE.",
    "BRIEF, BUT PRODUCTIVE."
};

static const char *const digs_miner_neutral_truce_broken[] = {
    "THOUGHT SO.",
    "NEVER SIGNED ANYTHING, %T.",
    "SHORT PEACE, %T. TYPICAL.",
    "IT WAS QUIET WHILE IT LASTED.",
    "STANDARD, THAT."
};

static const char *const digs_miner_thawing_hurt_by[] = {
    "AND WE WERE DOING SO WELL.",
    "THAT'S A STEP BACKWARDS, %T.",
    "I WAS ALMOST WARMING TO YOU.",
    "ONE. YOU GET ONE.",
    "THAT'S ON ME FOR HOPING."
};

static const char *const digs_miner_thawing_hurt_them[] = {
    "THAT WAS HALF WHAT I HAD.",
    "DON'T MAKE ME FINISH IT, %T.",
    "REFLEX. MOSTLY.",
    "I PULLED THAT ONE. NOTICE.",
    "WE CAN STOP ANY TIME, %T."
};

static const char *const digs_miner_thawing_killed_by[] = {
    "SO THAT'S WHERE WE STAND.",
    "SHOULD HAVE KNOWN, %T.",
    "AND I WAS BEING NICE.",
    "BACK TO THE OLD ARRANGEMENT.",
    "MY FAULT FOR TRYING."
};

static const char *const digs_miner_thawing_killed_them[] = {
    "SORRY. HALF SORRY.",
    "WE WERE NEARLY SOMETHING, %T.",
    "BAD TIMING, THAT'S ALL.",
    "I'D HAVE PREFERRED THE OTHER WAY.",
    "DON'T HOLD IT AGAINST ME, %T."
};

static const char *const digs_miner_thawing_saved_by[] = {
    "HUH. YOU. OF ALL PEOPLE.",
    "THAT COMPLICATES THINGS, %T.",
    "NOW I HAVE TO RETHINK YOU.",
    "DIDN'T SEE THAT COMING FROM YOU, %T.",
    "ALL RIGHT. WE'RE GETTING SOMEWHERE."
};

static const char *const digs_miner_thawing_spotted[] = {
    "YOU AGAIN. NOT REACHING FOR IT.",
    "EASY, %T. JUST LOOKING.",
    "I'M NOT STARTING ANYTHING TODAY.",
    "STILL DON'T LIKE YOU. STILL HERE.",
    "HANDS WHERE I CAN SEE THEM, %T."
};

static const char *const digs_miner_thawing_teamed_up[] = {
    "SEE, %T? NOT SO HARD.",
    "THAT WENT BETTER THAN LAST TIME.",
    "YOU'RE ALMOST USEFUL, %T.",
    "STILL WATCHING YOU. GOOD WORK.",
    "MIGHT DO THAT AGAIN."
};

static const char *const digs_miner_thawing_truce_broken[] = {
    "AND THERE IT IS.",
    "I NEARLY TRUSTED YOU, %T.",
    "SHOULD HAVE STAYED ANGRY.",
    "BACK TO WHERE WE STARTED, THEN.",
    "LESSON LEARNED."
};

static const char *const digs_miner_truce_hurt_by[] = {
    "TELL ME THAT WAS THE CEILING.",
    "%T. THINK CAREFULLY NOW.",
    "ONE MORE AND WE'RE DONE TALKING.",
    "I'M CHOOSING TO BELIEVE THAT SLIPPED.",
    "STILL NOT SHOOTING. STILL WAITING."
};

static const char *const digs_miner_truce_hurt_them[] = {
    "THAT WASN'T MEANT FOR YOU.",
    "BAD SWING. SORRY, %T.",
    "ACCIDENT. IT WAS AN ACCIDENT.",
    "DEAL'S STILL ON. THAT WAS THE ROCK.",
    "TAKE ONE BACK, %T. FAIR'S FAIR."
};

static const char *const digs_miner_truce_killed_by[] = {
    "SO THAT'S WHAT YOUR WORD IS WORTH.",
    "WE SHOOK ON IT, %T.",
    "I ACTUALLY BELIEVED YOU.",
    "NOTE FOR NEXT TIME. NO DEALS.",
    "THAT'S THE LAST HANDSHAKE YOU GET."
};

static const char *const digs_miner_truce_killed_them[] = {
    "THAT WASN'T THE ARRANGEMENT. SORRY.",
    "I'LL EXPLAIN WHEN YOU'RE UP, %T.",
    "THE LAVA WOULD HAVE DONE IT ANYWAY.",
    "TELL THEM IT WAS A MISUNDERSTANDING.",
    "WE'LL SORT IT OUT NEXT ROUND, %T."
};

static const char *const digs_miner_truce_saved_by[] = {
    "YOU DIDN'T HAVE TO GO THAT FAR.",
    "THE DEAL SAID DON'T SHOOT ME. NOT THAT.",
    "%T. THAT'S ABOVE THE ARRANGEMENT.",
    "RIGHT. THE DEAL STANDS. LONGER.",
    "CREDIT WHERE IT'S DUE, %T."
};

static const char *const digs_miner_truce_spotted[] = {
    "STILL HOLDING, %T?",
    "WALK ON. I WILL.",
    "NOTHING FROM ME. NOTHING FROM YOU.",
    "GOOD. WE BOTH KEPT OUR WORD.",
    "PICK ANOTHER TUNNEL, %T. WE'RE FINE."
};

static const char *const digs_miner_truce_teamed_up[] = {
    "SEE, %T? THIS IS WHY WE AGREED.",
    "GOOD DEAL, THIS ONE.",
    "TERMS EXTENDED, %T.",
    "WORKS BETTER THAN FIGHTING.",
    "SAME ARRANGEMENT, SAME RESULT."
};

static const char *const digs_miner_truce_truce_broken[] = {
    "RIGHT. THE DEAL'S DEAD.",
    "YOU SPENT THAT CHEAP, %T.",
    "SHOULD HAVE STAYED SUSPICIOUS.",
    "NO MORE HANDSHAKES DOWN HERE.",
    "THAT WAS THE ONLY PEACE I HAD."
};

static const char *const digs_miner_wary_humiliated[] = {
    "YOU'VE REALLY COMMITTED TO THIS.",
    "I KEEP GIVING YOU THE CHANCE, %T.",
    "THAT'S THE LAST OF MY GOODWILL.",
    "I'M DONE BEING SURPRISED, %T.",
    "WHATEVER WE HAD, IT'S GONE NOW."
};

static const char *const digs_miner_wary_hurt_by[] = {
    "SO THAT'S WHERE WE ARE.",
    "YOU MEANT THAT ONE, %T.",
    "I HOPED YOU'D THINK BETTER OF IT.",
    "THAT'S TWICE NOW, %T.",
    "RIGHT. LESSON LEARNED."
};

static const char *const digs_miner_wary_hurt_them[] = {
    "YOU MADE ME DO THAT.",
    "DON'T MAKE ME FINISH IT.",
    "THIS ISN'T HOW I WANTED THE SHIFT.",
    "BACK OFF AND WE'RE DONE, %T.",
    "I DIDN'T PUT MUCH BEHIND THAT."
};

static const char *const digs_miner_wary_killed_by[] = {
    "SO YOU WOULD. GOOD TO KNOW.",
    "I NEARLY TRUSTED YOU AGAIN, %T.",
    "THAT ANSWERS THAT, %T.",
    "I SHOULD HAVE SEEN IT COMING.",
    "NO MORE BENEFIT OF THE DOUBT."
};

static const char *const digs_miner_wary_killed_them[] = {
    "I DIDN'T WANT IT THIS WAY.",
    "SHOULD HAVE LEFT IT ALONE, %T.",
    "WE HAD SOMETHING WORKING.",
    "NOW I'VE GOT TO LIVE WITH THAT.",
    "THAT'S THE PARTNERSHIP DONE."
};

static const char *const digs_miner_wary_revenge[] = {
    "YOU MADE THAT EASY IN THE END.",
    "I DIDN'T WANT TO KNOW THAT ABOUT YOU.",
    "THAT'S US SETTLED, %T. NOTHING MORE.",
    "I TOOK NO PLEASURE IN IT.",
    "IT DIDN'T HAVE TO GO LIKE THAT."
};

static const char *const digs_miner_wary_spotted[] = {
    "KEEP YOUR DISTANCE, %T.",
    "I'M NOT TURNING MY BACK AGAIN.",
    "THAT'S FAR ENOUGH.",
    "WE USED TO SHARE A LAMP.",
    "I'M WATCHING YOUR HANDS, %T."
};

static const char *const digs_miner_wary_taunted[] = {
    "YOU NEVER USED TO TALK LIKE THAT.",
    "SAVE IT. I KNOW YOU BETTER.",
    "THAT'S NOT THE %T I WORKED WITH.",
    "SAYING IT LOUDER WON'T MEND IT.",
    "I'D RATHER YOU SAID NOTHING."
};

static const char *const digs_rivet_bonded_hurt_by[] = {
    "THAT WILL HAVE BEEN THE TERRAIN.",
    "I KNOW IT WAS NOT DELIBERATE, %T.",
    "NO HARM. WATCH YOUR SPREAD.",
    "THE ROCK CARRIED IT. NOT YOU.",
    "I HAVE HAD WORSE FROM MY OWN DRILL."
};

static const char *const digs_rivet_bonded_hurt_them[] = {
    "THAT WAS NOT MEANT FOR YOU, %T.",
    "BADLY DONE. MINE ENTIRELY.",
    "GET BEHIND THE PILLAR. NOW.",
    "I MISJUDGED THE CHARGE. SORRY.",
    "STAY BACK UNTIL I HAVE RESET IT."
};

static const char *const digs_rivet_bonded_killed_by[] = {
    "THEN IT HAD TO BE YOU, %T.",
    "I DO NOT MIND. FROM YOU.",
    "MAKE IT COUNT FOR SOMETHING.",
    "NO REVISION. WE ARE STILL SQUARE.",
    "GOOD SHOT. I TAUGHT YOU THAT ONE."
};

static const char *const digs_rivet_bonded_killed_them[] = {
    "THAT SHOULD NOT HAVE HAPPENED.",
    "%T. I AM SORRY. GENUINELY.",
    "THE LAVA WAS GOING TO TAKE YOU.",
    "I HATED EVERY PART OF THAT.",
    "IT WAS THE SHIFT, NOT ME."
};

static const char *const digs_rivet_bonded_saved_by[] = {
    "THAT IS TWICE NOW, %T.",
    "I HAD THE FIGURES WRONG. YOU DID NOT.",
    "I WILL NOT FORGET THE TIMING.",
    "YOU READ THAT ROOF BEFORE I DID.",
    "COMPETENT. THAT IS HIGH PRAISE."
};

static const char *const digs_rivet_bonded_spotted[] = {
    "%T. GOOD. I WAS SHORT A PAIR OF HANDS.",
    "THE EAST FACE IS YOURS. AS ALWAYS.",
    "THERE YOU ARE. THE ROOF IS BAD.",
    "MIND THE SPAN AHEAD OF YOU, %T.",
    "I KEPT YOUR SIDE OF THE TUNNEL CLEAR."
};

static const char *const digs_rivet_bonded_teamed_up[] = {
    "TEXTBOOK, %T.",
    "THAT IS HOW THE SURVEY SHOULD READ.",
    "WE HAVE DONE THIS BEFORE.",
    "NOTHING TO ADD. THAT WAS CORRECT.",
    "BEST WORK ON THIS SHIFT."
};

static const char *const digs_rivet_bonded_truce_broken[] = {
    "NO. NOT FROM YOU, %T.",
    "THERE MUST BE A REASON. TELL ME.",
    "I WILL WAIT BEFORE I REVISE.",
    "THAT IS THE ONE ENTRY I DID NOT WANT.",
    "EXPLAIN IT. I WILL LISTEN FIRST."
};

static const char *const digs_rivet_feud_humiliated[] = {
    "THIS IS BECOMING A PATTERN, %T.",
    "I HAVE REVISED MY ESTIMATE OF YOU.",
    "EVERY SHIFT. THE SAME SIGNATURE.",
    "I AM NOT LEARNING. I AM COUNTING.",
    "ONE OF THESE THE FIGURES TURN."
};

static const char *const digs_rivet_feud_hurt_by[] = {
    "ADD IT. I KEEP A COLUMN FOR YOU.",
    "STILL WITHIN TOLERANCE, %T.",
    "YOU HAVE NEVER SURPRISED ME ONCE.",
    "NOTED. THE TOTAL IS UNCHANGED.",
    "I FELT THAT. I WILL RETURN IT."
};

static const char *const digs_rivet_feud_hurt_them[] = {
    "THAT WAS OWED WITH INTEREST.",
    "I HAVE HAD MONTHS TO REFINE THAT.",
    "LOGGED. AND I AM STILL BEHIND.",
    "YOU ARE EASIER TO SOLVE EVERY SHIFT.",
    "THAT WAS NOT AN OPENING SHOT, %T."
};

static const char *const digs_rivet_feud_killed_by[] = {
    "YOU DO NOT GET TO END IT, %T.",
    "THE COLUMN GROWS. SO DO I.",
    "I WILL BE ALONG. I ALWAYS AM.",
    "THAT CHANGES ONE NUMBER. ONE.",
    "MISCALCULATION. NOT A CONCESSION."
};

static const char *const digs_rivet_feud_killed_them[] = {
    "THAT ENTRY IS FINALLY CLOSED.",
    "I HAVE WANTED THAT SINCE THE FIRST.",
    "GOODBYE, %T. I MEAN THE WHOLE OF IT.",
    "NO SURVEY REQUIRED. I KNEW THE SPOT.",
    "THE LEDGER READS BETTER ALREADY."
};

static const char *const digs_rivet_feud_revenge[] = {
    "BALANCED. AND STILL NOT ENOUGH.",
    "I SAID I WOULD CORRECT THAT, %T.",
    "THAT WAS ARITHMETIC, NOT TEMPER.",
    "YEARS OF IT, PAID IN ONE STROKE.",
    "THE ACCOUNT READS ZERO. FOR NOW."
};

static const char *const digs_rivet_feud_spotted[] = {
    "YOU ARE IN THE MARGIN OF MY SURVEY.",
    "%T. I KNOW YOUR STRIDE LENGTH NOW.",
    "I HAVE BEEN EXPECTING THIS BEARING.",
    "THERE IS NOTHING CASUAL LEFT HERE.",
    "I DO NOT NEED TO LOOK TWICE, %T."
};

static const char *const digs_rivet_feud_taunted[] = {
    "I STOPPED FILING YOUR REMARKS.",
    "SAVE THE AIR FOR THE CLIMB, %T.",
    "WORDS DO NOT MOVE ROCK.",
    "YOU TALK MORE THAN YOU DIG.",
    "IRRELEVANT. AS EVER."
};

static const char *const digs_rivet_hostile_humiliated[] = {
    "THREE. THE PATTERN IS CLEAR.",
    "YOU HAVE A METHOD. I WILL FIND IT.",
    "MY ESTIMATE OF YOU WAS LOW.",
    "THIS IS DATA, %T. I USE DATA.",
    "THE NEXT ONE GOES DIFFERENTLY."
};

static const char *const digs_rivet_hostile_hurt_by[] = {
    "ACCEPTED. IT CHANGES THE METHOD.",
    "INEFFICIENT, BUT IT LANDED.",
    "STRUCTURAL DAMAGE. NOT CRITICAL.",
    "YOU ARE WORKING HARDER THAN I AM.",
    "NOTED, %T. PROCEEDING."
};

static const char *const digs_rivet_hostile_hurt_them[] = {
    "ON TARGET. ADJUSTING FOR THE NEXT.",
    "TWO MORE AND THE SPAN COMES DOWN.",
    "YOU ARE WITHIN THE BLAST FIGURE.",
    "GOOD GROUPING. NOT MY BEST.",
    "THAT WAS THE RANGING SHOT, %T."
};

static const char *const digs_rivet_hostile_killed_by[] = {
    "MISCALCULATION. I WILL REVISE.",
    "THAT WAS AN ERROR IN MY FIGURES.",
    "LOGGED, %T. WE CONTINUE.",
    "UNEXPECTED. NOT UNSURVIVABLE.",
    "THE SURVEY WAS INCOMPLETE."
};

static const char *const digs_rivet_hostile_killed_them[] = {
    "SECTION CLEARED.",
    "SHIFT CONCLUDED FOR %T.",
    "FILED UNDER COMPLETED WORK.",
    "AS SURVEYED. AS EXPECTED.",
    "THE TUNNEL IS MINE AGAIN."
};

static const char *const digs_rivet_hostile_revenge[] = {
    "ACCOUNTS SETTLED. BACK TO WORK.",
    "THAT WAS OWED FROM EARLIER.",
    "CORRECTED. THE FIGURES AGREE NOW.",
    "ONE FOR ONE, %T. TIDY.",
    "THE ERROR HAS BEEN AMENDED."
};

static const char *const digs_rivet_hostile_spotted[] = {
    "OBSTRUCTION IN THE WORKING FACE.",
    "%T. IN THE WAY, AS USUAL.",
    "CLEARING THIS SECTION NOW.",
    "NOTHING PERSONAL. JUST SCHEDULED.",
    "HOLD STILL. IT IS QUICKER."
};

static const char *const digs_rivet_hostile_taunted[] = {
    "YOUR AIR IS BETTER SPENT RUNNING.",
    "I AM MEASURING, NOT LISTENING.",
    "TALK IS CHEAP AT THIS DEPTH.",
    "MM. CONTINUE IF IT HELPS."
};

static const char *const digs_rivet_needling_humiliated[] = {
    "THIS IS NO LONGER AMUSING.",
    "YOU HAVE A METHOD AFTER ALL, %T.",
    "I AM REVISING UPWARD. SLIGHTLY.",
    "THAT IS ENOUGH OF THAT.",
    "I WILL START TAKING NOTES."
};

static const char *const digs_rivet_needling_hurt_by[] = {
    "AN ACCIDENT, BUT I WILL ALLOW IT.",
    "YOU HIT SOMETHING. CONGRATULATIONS.",
    "INTERESTING. DO IT ON PURPOSE NEXT.",
    "THAT WAS WITHIN THE MARGIN, %T.",
    "STATISTICALLY, ONE HAD TO LAND."
};

static const char *const digs_rivet_needling_hurt_them[] = {
    "PREDICTABLE. ALMOST RESTFUL.",
    "YOU LEAN LEFT WHEN YOU PANIC, %T.",
    "I HAVE NOT ADJUSTED SINCE THE FIRST.",
    "THAT WAS THE EASY SOLUTION.",
    "YOUR COVER CHOICE IS POOR."
};

static const char *const digs_rivet_needling_killed_by[] = {
    "HUH. THE FIGURES SAY OTHERWISE.",
    "YOU GOT THE ANGLE. ONCE.",
    "I WILL WANT THAT ONE EXPLAINED.",
    "A GOOD SHOT FROM A POOR MINER.",
    "NOTED, %T. WITH SOME SURPRISE."
};

static const char *const digs_rivet_needling_killed_them[] = {
    "THAT REQUIRED NO PLANNING.",
    "YOU STOOD ON THE UNSUPPORTED SPAN.",
    "READ THE ROOF NEXT TIME, %T.",
    "THE ROCK DID MOST OF THAT.",
    "FILED. UNDER STRAIGHTFORWARD."
};

static const char *const digs_rivet_needling_revenge[] = {
    "ORDER RESTORED.",
    "THAT IS THE CORRECT ARRANGEMENT.",
    "YOU WERE ABOVE YOUR GRADE, %T.",
    "ONE EACH. MINE WAS TIDIER.",
    "THE ANOMALY HAS BEEN RESOLVED."
};

static const char *const digs_rivet_needling_spotted[] = {
    "THERE IS THE UNDERGROUND HAZARD.",
    "%T. STILL DIGGING IT WRONG.",
    "I CAN HEAR YOUR FOOTWORK FROM HERE.",
    "THAT IS NOT HOW YOU HOLD IT.",
    "OH. THE ENTHUSIAST."
};

static const char *const digs_rivet_needling_taunted[] = {
    "THAT NEARLY PARSED.",
    "SAY IT AGAIN WITH THE FACTS IN.",
    "YOU SHOULD WRITE THEM DOWN FIRST.",
    "MM. NO.",
    "I HAVE HEARD BETTER FROM THE PUMPS."
};

static const char *const digs_rivet_neutral_hurt_by[] = {
    "WITHIN TOLERANCE.",
    "MINOR STRUCTURAL LOSS.",
    "I WILL COMPENSATE."
};

static const char *const digs_rivet_neutral_hurt_them[] = {
    "ADJUSTING TWO DEGREES.",
    "THAT IS ONE, %T.",
    "ACCEPTABLE GROUPING.",
    "THE FIGURES HOLD."
};

static const char *const digs_rivet_neutral_killed_by[] = {
    "I WILL REVISE THE SURVEY.",
    "LOGGED, %T.",
    "UNEXPECTED VARIABLE."
};

static const char *const digs_rivet_neutral_killed_them[] = {
    "FILED, %T.",
    "CLEAN. NEXT SECTION.",
    "THE WORKING IS CLEAR."
};

static const char *const digs_rivet_neutral_saved_by[] = {
    "THAT WAS EFFICIENT OF YOU.",
    "UNEXPECTED. NOTED, %T.",
    "I HAD NOT ACCOUNTED FOR THAT.",
    "OBLIGED.",
    "YOUR TIMING WAS SOUND."
};

static const char *const digs_rivet_neutral_spotted[] = {
    "CONTACT. BEARING NOTED.",
    "%T IS IN THE WORKING.",
    "ONE LAMP. RANGE CLOSING.",
    "LOGGED. PROCEEDING WITH CAUTION."
};

static const char *const digs_rivet_neutral_teamed_up[] = {
    "GOOD ANGLE, %T.",
    "THAT WORKED. REPEAT IT.",
    "COORDINATED. UNUSUAL.",
    "THE SPAN CAME DOWN NICELY.",
    "ACCEPTABLE WORK."
};

static const char *const digs_rivet_neutral_truce_broken[] = {
    "THE ARRANGEMENT IS VOID.",
    "SO MUCH FOR THAT, %T.",
    "BACK TO THE ORIGINAL FIGURES.",
    "NOTED. TERMS WITHDRAWN.",
    "I DID NOT EXPECT IT TO HOLD."
};

static const char *const digs_rivet_thawing_hurt_by[] = {
    "I WILL ASSUME THAT WAS ACCIDENTAL.",
    "ONE MORE AND I REVISE BACK DOWN.",
    "NOTED. NOT YET LOGGED, %T.",
    "THE BENEFIT OF THE DOUBT. ONCE.",
    "CAREFUL. I WAS ALMOST CIVIL."
};

static const char *const digs_rivet_thawing_hurt_them[] = {
    "REFLEX. NOT INTENT.",
    "I WILL CALL THAT A MISFIRE, %T.",
    "APOLOGIES. PARTIAL ONES.",
    "OLD HABIT. BEING CORRECTED.",
    "THAT WAS BEFORE I RECONSIDERED."
};

static const char *const digs_rivet_thawing_killed_by[] = {
    "AND I WAS BEING REASONABLE.",
    "BACK TO THE OLD FIGURES, THEN.",
    "THAT IS A SHAME, %T.",
    "MY REVISION WAS PREMATURE.",
    "NOTED. WITH SOME DISAPPOINTMENT."
};

static const char *const digs_rivet_thawing_killed_them[] = {
    "THAT UNDOES SOME PROGRESS.",
    "REGRETTABLE, %T. GENUINELY.",
    "THE SITUATION REQUIRED IT.",
    "I WOULD RATHER HAVE NOT FILED THAT.",
    "WE WERE GETTING SOMEWHERE."
};

static const char *const digs_rivet_thawing_saved_by[] = {
    "THAT WAS NOT REQUIRED OF YOU.",
    "I AM ADJUSTING MY ESTIMATE, %T.",
    "UNEXPECTED. AND USEFUL.",
    "I WILL REMEMBER THE TIMING.",
    "OBLIGED. PROPERLY, THIS TIME."
};

static const char *const digs_rivet_thawing_spotted[] = {
    "%T. NOT SHOOTING. YET.",
    "I AM REVISING MY ESTIMATE OF YOU.",
    "HANDS VISIBLE AND WE HAVE NO PROBLEM.",
    "YOU HAVE BEEN LESS TROUBLE LATELY.",
    "BEARING NOTED. WEAPON LOWERED."
};

static const char *const digs_rivet_thawing_teamed_up[] = {
    "THAT WAS WELL ANGLED, %T.",
    "WE WORK BETTER THAN WE FIGHT.",
    "REPEAT THAT ARRANGEMENT.",
    "THE FIGURES FAVOUR COOPERATION.",
    "GOOD. KEEP THE LINE."
};

static const char *const digs_rivet_thawing_truce_broken[] = {
    "I KNEW THE MARGIN WAS THIN.",
    "BACK TO THE ORIGINAL ESTIMATE, %T.",
    "THAT DID NOT LAST LONG.",
    "THE REVISION IS WITHDRAWN.",
    "I SHOULD HAVE HELD MY FIGURES."
};

static const char *const digs_rivet_truce_hurt_by[] = {
    "THE TERMS, %T. MIND THEM.",
    "I WILL TREAT THAT AS SPILLAGE.",
    "ONE MORE AND WE RENEGOTIATE.",
    "THAT WAS CLOSE TO A BREACH.",
    "EXPLAIN THAT ONE. QUICKLY."
};

static const char *const digs_rivet_truce_hurt_them[] = {
    "THAT WAS NOT AIMED AT YOU, %T.",
    "BLAST RADIUS. NOT INTENT.",
    "THE TERMS STAND. THAT WAS TERRAIN.",
    "APOLOGIES. THE CHARGE OVERRAN.",
    "I MISCALCULATED THE SPREAD."
};

static const char *const digs_rivet_truce_killed_by[] = {
    "SO THE TERMS MEANT NOTHING.",
    "I HELD MY SIDE, %T.",
    "AN ERROR OF JUDGEMENT. MINE.",
    "THAT IS A PERMANENT REVISION.",
    "NOTED. THE ARRANGEMENT IS DEAD."
};

static const char *const digs_rivet_truce_killed_them[] = {
    "THE ARRANGEMENT COULD NOT HOLD.",
    "THAT WAS NOT HOW I WANTED IT, %T.",
    "THE LAVA WOULD HAVE DONE IT ANYWAY.",
    "I HELD THE TERMS AS LONG AS I COULD.",
    "FILED. UNDER UNAVOIDABLE."
};

static const char *const digs_rivet_truce_saved_by[] = {
    "THE TERMS ARE WORKING, %T.",
    "THAT WAS BEYOND THE AGREEMENT.",
    "I WILL RETURN IT AT THE FIRST CHANCE.",
    "GOOD. THE ARRANGEMENT PAYS.",
    "OBLIGED. AND IT IS RECORDED."
};

static const char *const digs_rivet_truce_spotted[] = {
    "%T. THE TERMS STILL HOLD.",
    "LAMP DOWN. NOTHING TO SETTLE HERE.",
    "YOU HAVE THE EAST WORKING. AGREED.",
    "PASSING THROUGH, NOTHING MORE.",
    "THE ARRANGEMENT IS INTACT."
};

static const char *const digs_rivet_truce_teamed_up[] = {
    "THE TERMS EXTEND NICELY, %T.",
    "TWO SURVEYS AGREE. USEFUL.",
    "THAT IS WHAT THE AGREEMENT IS FOR.",
    "KEEP THAT LINE AND WE BOTH WALK.",
    "EFFICIENT. VERY EFFICIENT."
};

static const char *const digs_rivet_truce_truce_broken[] = {
    "THE TERMS ARE VOID, %T.",
    "YOU HAD ONE CONDITION. ONE.",
    "I WILL NOT OFFER THAT AGAIN.",
    "RECORDED. AND NOT FORGOTTEN."
};

static const char *const digs_rivet_wary_humiliated[] = {
    "YOU ARE ENJOYING THIS. I SEE IT.",
    "I KEPT GIVING YOU THE BENEFIT, %T.",
    "THE GENEROUS ESTIMATE IS WITHDRAWN.",
    "THAT IS SUFFICIENT. TRULY.",
    "I WAS CIVIL WITH YOU. ONCE."
};

static const char *const digs_rivet_wary_hurt_by[] = {
    "SO THAT IS THE NEW ARRANGEMENT.",
    "YOU AIMED, %T. YOU ACTUALLY AIMED.",
    "DISAPPOINTING. NOT SURPRISING NOW.",
    "MY ASSESSMENT WAS GENEROUS.",
    "THE ACCOUNT IS REOPENED, THEN."
};

static const char *const digs_rivet_wary_hurt_them[] = {
    "YOU MADE THAT NECESSARY.",
    "I TOOK NO SATISFACTION IN IT, %T.",
    "THAT IS FAR ENOUGH. STOP THERE.",
    "I WOULD RATHER HAVE NOT.",
    "CONSIDER IT A WARNING SHOT."
};

static const char *const digs_rivet_wary_killed_by[] = {
    "FROM YOU. THAT IS THE PART.",
    "I HAD REVISED MY ESTIMATE UPWARD.",
    "THAT WAS BADLY DONE, %T.",
    "AN ERROR OF TRUST, NOT OF MATH.",
    "I WILL NOT MAKE IT TWICE."
};

static const char *const digs_rivet_wary_killed_them[] = {
    "I DID NOT WANT THAT ENTRY.",
    "THAT IS A WASTE OF A GOOD MINER.",
    "YOU SHOULD HAVE HELD THE LINE, %T.",
    "NOBODY WINS THAT ONE.",
    "FILED. WITHOUT SATISFACTION."
};

static const char *const digs_rivet_wary_revenge[] = {
    "LEVEL. AND WORSE FOR BOTH.",
    "I TOOK NO PLEASURE IN THAT, %T.",
    "THE FIGURES AGREE. NOTHING ELSE DOES.",
    "THAT IS THE LAST WARM ENTRY.",
    "NOW WE BOTH KNOW THE GROUND."
};

static const char *const digs_rivet_wary_spotted[] = {
    "I AM KEEPING THE PILLAR BETWEEN US.",
    "%T. AT A DISTANCE, PLEASE.",
    "YOUR LAMP IS CLOSER THAN I LIKE.",
    "I HAD YOU DOWN AS RELIABLE.",
    "STAY WHERE THE SURVEY SEES YOU."
};

static const char *const digs_rivet_wary_taunted[] = {
    "THAT IS A NEW REGISTER FOR YOU.",
    "YOU DID NOT USED TO TALK LIKE THAT.",
    "I PREFERRED THE SILENCE, %T.",
    "SAY SOMETHING TRUE AND I WILL LISTEN.",
    "MM. NOT FROM YOU."
};

static const char *const digs_cinder_feud_truce_accepted[] = {
    "FINE! TRUCE! DON'T TOUCH ME!",
    "I AM NOT SHAKING YOUR HAND, %T!",
    "DEAL. I AM STILL ANGRY.",
    "GOOD! NOW POINT ME AT SOMEBODY ELSE!",
    "TRUCE! FOR NOW! FOR NOW!"
};

static const char *const digs_cinder_feud_truce_offered[] = {
    "RIGHT. I AM BORED OF HATING YOU.",
    "TRUCE, %T! I SAID TRUCE!",
    "THIS IS GETTING US NOWHERE! STOP!",
    "PUT IT DOWN AND SO WILL I!",
    "I STILL HATE YOU! BUT TRUCE!"
};

static const char *const digs_cinder_neutral_truce_accepted[] = {
    "HA! GOOD! COME ON THEN!",
    "DEAL, %T! NOW KEEP UP!",
    "TRUCE! I LIKE THIS ALREADY!",
    "RIGHT! WHO ARE WE HITTING!",
    "DONE! DON'T MAKE ME REGRET IT!"
};

static const char *const digs_cinder_neutral_truce_offered[] = {
    "OI! TRUCE! I WANT THE BIG ONE!",
    "YOU AND ME, %T? AGAINST THEM?",
    "SAVE IT! THERE'S BETTER MEAT!",
    "TRUCE! FOR TEN MINUTES!",
    "I'LL NOT SWING IF YOU'LL NOT!"
};

static const char *const digs_cinder_thawing_truce_accepted[] = {
    "HA! I KNEW YOU'D COME ROUND!",
    "GOOD LAD, %T! GOOD LAD!",
    "TRUCE! NOW STAND BEHIND ME!",
    "DONE! THAT WAS EASY!",
    "RIGHT! FRIENDS! FOR NOW!"
};

static const char *const digs_cinder_thawing_truce_offered[] = {
    "YOU'RE ALL RIGHT, ACTUALLY. TRUCE?",
    "I'VE STOPPED WANTING TO HIT YOU, %T!",
    "TRUCE! I MEAN IT THIS TIME!",
    "COME ON. WE BOTH KNOW IT'S OVER.",
    "SHAKE ON IT OR I'LL SWING!"
};

static const char *const digs_cinder_truce_truce_accepted[] = {
    "THAT'S THE SPIRIT!",
    "STILL US TWO! HA!",
    "GOOD! NOW WATCH MY BACK!",
    "KNEW IT! COME ON!",
    "DEAL! AGAIN! ALWAYS!"
};

static const char *const digs_cinder_truce_truce_offered[] = {
    "STILL US TWO, %T? STILL US TWO.",
    "KEEP IT GOING! IT'S WORKING!",
    "NO REASON TO STOP NOW!",
    "SAME AS BEFORE! YES?",
    "YOU'RE WITH ME! SAY YOU'RE WITH ME!"
};

static const char *const digs_flamey_feud_truce_accepted[] = {
    "OOH! WE'RE FRIENDS NOW! TERRIFYING.",
    "DON'T STAND TOO CLOSE, %T.",
    "TRUCE! I'LL HOLD THE MATCHES.",
    "LOVELY. I'LL BE RIGHT BEHIND YOU.",
    "AGREED. WATCH YOUR POCKETS."
};

static const char *const digs_flamey_feud_truce_offered[] = {
    "TRUCE? SAY NO. GO ON. SAY NO.",
    "I'M OFFERING, %T. LOOK HOW NICE I AM.",
    "LET'S STOP. I'M RUNNING LOW ON MATCHES.",
    "TRUCE. NO TRICKS. PROBABLY NO TRICKS.",
    "EVEN I'M TIRED OF THIS ONE."
};

static const char *const digs_flamey_neutral_truce_accepted[] = {
    "WONDERFUL! THIS WILL END WELL!",
    "PARTNERS, %T! HOW EXCITING!",
    "TRUCE! I'VE ALWAYS WANTED ONE!",
    "DONE! NOW WATCH THIS.",
    "LOVELY. I'LL BEHAVE. MOSTLY."
};

static const char *const digs_flamey_neutral_truce_offered[] = {
    "TRUCE? I'VE GOT A BETTER IDEA ANYWAY.",
    "YOU AND ME, %T. AGAINST THE WORLD.",
    "LET'S NOT. LET'S DO SOMETHING FUN.",
    "I'LL BE GOOD. FOR A WHILE.",
    "PEACE! WITH CONDITIONS! LOTS OF THEM!"
};

static const char *const digs_flamey_thawing_truce_accepted[] = {
    "OH GOOD! I HATED BEING CROSS!",
    "FRIENDS, %T! I'M KEEPING THAT.",
    "TRUCE! THIS IS MUCH MORE FUN!",
    "SEE? I KNEW YOU'D COME ROUND.",
    "AGREED! NOW LET'S RUIN SOMEBODY'S DAY."
};

static const char *const digs_flamey_thawing_truce_offered[] = {
    "I'VE DECIDED I LIKE YOU. TRUCE?",
    "STOP MAKING IT HARD TO HATE YOU, %T.",
    "TRUCE. I'M AS SURPRISED AS YOU.",
    "LET'S BE FRIENDS. IT'LL ANNOY THEM.",
    "I'LL STOP IF YOU STOP. HONEST."
};

static const char *const digs_flamey_truce_truce_accepted[] = {
    "STILL FRIENDS! LOVELY!",
    "GOOD, %T. I'D HATE TO BURN YOU.",
    "KEEPING IT! I'M KEEPING IT!",
    "WONDERFUL. NOW HOLD THIS.",
    "SEE? EASY. WHY IS EVERYONE ELSE HARD."
};

static const char *const digs_flamey_truce_truce_offered[] = {
    "STILL FRIENDS? SAY STILL FRIENDS.",
    "US AGAINST THE MINE, %T.",
    "NOTHING'S CHANGED. HAS IT? HAS IT?",
    "SAME ARRANGEMENT! I LIKE IT!",
    "DON'T GO ODD ON ME NOW."
};

static const char *const digs_miner_feud_truce_accepted[] = {
    "RIGHT. TRUCE.",
    "I'M NOT FORGETTING IT, %T.",
    "DONE. DON'T PUSH IT.",
    "ALL RIGHT. BACK TO WORK.",
    "FINE. BUT I'M WATCHING YOU."
};

static const char *const digs_miner_feud_truce_offered[] = {
    "I'M DONE. ARE YOU DONE?",
    "THIS ISN'T WORTH IT, %T.",
    "CALL IT. WE BOTH GO HOME.",
    "PUT IT DOWN. I WILL TOO.",
    "I'D RATHER DIG THAN DO THIS AGAIN."
};

static const char *const digs_miner_neutral_truce_accepted[] = {
    "RIGHT. TRUCE IT IS.",
    "SUITS ME, %T.",
    "GOOD. LESS PAPERWORK.",
    "DONE. MIND THE ROOF.",
    "THAT'LL DO."
};

static const char *const digs_miner_neutral_truce_offered[] = {
    "TRUCE? THERE'S ENOUGH ROCK.",
    "NO ARGUMENT HERE, %T.",
    "I'LL LEAVE YOU BE.",
    "EASIER FOR BOTH OF US.",
    "YOU DIG YOURS. I'LL DIG MINE."
};

static const char *const digs_miner_thawing_truce_accepted[] = {
    "GOOD. THAT'S A WEIGHT OFF.",
    "ALL RIGHT THEN, %T.",
    "TRUCE. ABOUT TIME.",
    "GLAD THAT'S SORTED.",
    "RIGHT. LET'S GET ON WITH IT."
};

static const char *const digs_miner_thawing_truce_offered[] = {
    "YOU'RE NOT SO BAD. TRUCE?",
    "LET'S CALL IT, %T.",
    "I'M TIRED OF BEING ANGRY.",
    "CLEAN SLATE? WORTH A TRY.",
    "NO HARD FEELINGS. IF YOU'LL HAVE IT."
};

static const char *const digs_miner_truce_truce_accepted[] = {
    "STILL GOOD.",
    "AYE, %T.",
    "THAT'S US, THEN.",
    "GOOD. WATCH YOUR BACK OUT THERE.",
    "RIGHT. ON WE GO."
};

static const char *const digs_miner_truce_truce_offered[] = {
    "STILL GOOD, %T?",
    "SAME AS BEFORE.",
    "NO REASON TO CHANGE IT.",
    "WE'RE ALL RIGHT, YOU AND ME.",
    "KEEP IT GOING?"
};

static const char *const digs_rivet_feud_truce_accepted[] = {
    "TERMS ACCEPTED. AGAINST MY JUDGEMENT.",
    "THE LEDGER STAYS OPEN, %T. BUT PAUSED.",
    "AGREED. I AM NOT FORGETTING ANY OF IT.",
    "FINE. THE EAST FACE IS YOURS.",
    "RECORDED. DO NOT MAKE ME REVISE IT."
};

static const char *const digs_rivet_feud_truce_offered[] = {
    "I AM PROPOSING TERMS. ONCE.",
    "THIS IS COSTING US BOTH, %T.",
    "NEITHER OF US IS WINNING THE ARITHMETIC.",
    "STOP. WE BOTH WALK OUT.",
    "I WILL PUT DOWN THE RAIL IF YOU DO."
};

static const char *const digs_rivet_neutral_truce_accepted[] = {
    "AGREED. TERMS HOLD.",
    "NOTED, %T. EAST IS YOURS.",
    "SENSIBLE. BACK TO WORK.",
    "ACCEPTED. MIND THE SPAN.",
    "GOOD. THE SURVEY CAN CONTINUE."
};

static const char *const digs_rivet_neutral_truce_offered[] = {
    "A PROPOSAL. YOU TAKE EAST, I TAKE WEST.",
    "NO PROFIT IN THIS, %T. TERMS?",
    "I HAVE BETTER USES FOR THE CHARGE.",
    "SAY THE WORD AND I STAND DOWN.",
    "EFFICIENT OPTION. WE BOTH DIG."
};

static const char *const digs_rivet_thawing_truce_accepted[] = {
    "AGREED. AND NOTED PROPERLY.",
    "THAT WAS OVERDUE, %T.",
    "ACCEPTED. THE REVISION STANDS.",
    "GOOD. I HAD RUN THE NUMBERS ALREADY.",
    "TERMS. I WILL KEEP THEM."
};

static const char *const digs_rivet_thawing_truce_offered[] = {
    "I AM REVISING UPWARD. TERMS?",
    "YOU HAVE BEEN LESS TROUBLE, %T. TERMS.",
    "THE FIGURES FAVOUR AN ARRANGEMENT.",
    "I WOULD RATHER NOT SHOOT YOU TODAY.",
    "CALL IT. I WILL HOLD MY SIDE."
};

static const char *const digs_rivet_truce_truce_accepted[] = {
    "RENEWED. GOOD.",
    "STILL AGREED, THEN, %T.",
    "THE ARRANGEMENT HOLDS.",
    "SOUND. BACK TO THE FACE.",
    "I HAD ASSUMED SO. GOOD."
};

static const char *const digs_rivet_truce_truce_offered[] = {
    "THE TERMS EXTEND. SAY IF NOT.",
    "STILL AGREED, %T?",
    "I SEE NO REASON TO REOPEN IT.",
    "WE RENEW ON THE SAME FIGURES.",
    "NOTHING HAS CHANGED. HAS IT."
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
    DIGS_SET(digs_miner_doomed),                     /* 80 */
    DIGS_SET(digs_cinder_bonded_hurt_by),
    DIGS_SET(digs_cinder_bonded_hurt_them),
    DIGS_SET(digs_cinder_bonded_killed_by),
    DIGS_SET(digs_cinder_bonded_killed_them),
    DIGS_SET(digs_cinder_bonded_saved_by),
    DIGS_SET(digs_cinder_bonded_spotted),
    DIGS_SET(digs_cinder_bonded_teamed_up),
    DIGS_SET(digs_cinder_bonded_truce_broken),
    DIGS_SET(digs_cinder_feud_humiliated),
    DIGS_SET(digs_cinder_feud_hurt_by),
    DIGS_SET(digs_cinder_feud_hurt_them),
    DIGS_SET(digs_cinder_feud_killed_by),
    DIGS_SET(digs_cinder_feud_killed_them),
    DIGS_SET(digs_cinder_feud_revenge),
    DIGS_SET(digs_cinder_feud_spotted),
    DIGS_SET(digs_cinder_feud_taunted),
    DIGS_SET(digs_cinder_hostile_humiliated),
    DIGS_SET(digs_cinder_hostile_hurt_by),
    DIGS_SET(digs_cinder_hostile_hurt_them),
    DIGS_SET(digs_cinder_hostile_killed_by),
    DIGS_SET(digs_cinder_hostile_killed_them),
    DIGS_SET(digs_cinder_hostile_revenge),
    DIGS_SET(digs_cinder_hostile_spotted),
    DIGS_SET(digs_cinder_hostile_taunted),
    DIGS_SET(digs_cinder_needling_humiliated),
    DIGS_SET(digs_cinder_needling_hurt_by),
    DIGS_SET(digs_cinder_needling_hurt_them),
    DIGS_SET(digs_cinder_needling_killed_by),
    DIGS_SET(digs_cinder_needling_killed_them),
    DIGS_SET(digs_cinder_needling_revenge),
    DIGS_SET(digs_cinder_needling_spotted),
    DIGS_SET(digs_cinder_needling_taunted),
    DIGS_SET(digs_cinder_neutral_hurt_by),
    DIGS_SET(digs_cinder_neutral_hurt_them),
    DIGS_SET(digs_cinder_neutral_killed_by),
    DIGS_SET(digs_cinder_neutral_killed_them),
    DIGS_SET(digs_cinder_neutral_saved_by),
    DIGS_SET(digs_cinder_neutral_spotted),
    DIGS_SET(digs_cinder_neutral_teamed_up),
    DIGS_SET(digs_cinder_neutral_truce_broken),
    DIGS_SET(digs_cinder_thawing_hurt_by),
    DIGS_SET(digs_cinder_thawing_hurt_them),
    DIGS_SET(digs_cinder_thawing_killed_by),
    DIGS_SET(digs_cinder_thawing_killed_them),
    DIGS_SET(digs_cinder_thawing_saved_by),
    DIGS_SET(digs_cinder_thawing_spotted),
    DIGS_SET(digs_cinder_thawing_teamed_up),
    DIGS_SET(digs_cinder_thawing_truce_broken),
    DIGS_SET(digs_cinder_truce_hurt_by),
    DIGS_SET(digs_cinder_truce_hurt_them),
    DIGS_SET(digs_cinder_truce_killed_by),
    DIGS_SET(digs_cinder_truce_killed_them),
    DIGS_SET(digs_cinder_truce_saved_by),
    DIGS_SET(digs_cinder_truce_spotted),
    DIGS_SET(digs_cinder_truce_teamed_up),
    DIGS_SET(digs_cinder_truce_truce_broken),
    DIGS_SET(digs_cinder_wary_humiliated),
    DIGS_SET(digs_cinder_wary_hurt_by),
    DIGS_SET(digs_cinder_wary_hurt_them),
    DIGS_SET(digs_cinder_wary_killed_by),
    DIGS_SET(digs_cinder_wary_killed_them),
    DIGS_SET(digs_cinder_wary_revenge),
    DIGS_SET(digs_cinder_wary_spotted),
    DIGS_SET(digs_cinder_wary_taunted),
    DIGS_SET(digs_flamey_bonded_hurt_by),
    DIGS_SET(digs_flamey_bonded_hurt_them),
    DIGS_SET(digs_flamey_bonded_killed_by),
    DIGS_SET(digs_flamey_bonded_killed_them),
    DIGS_SET(digs_flamey_bonded_saved_by),
    DIGS_SET(digs_flamey_bonded_spotted),
    DIGS_SET(digs_flamey_bonded_teamed_up),
    DIGS_SET(digs_flamey_bonded_truce_broken),
    DIGS_SET(digs_flamey_feud_humiliated),
    DIGS_SET(digs_flamey_feud_hurt_by),
    DIGS_SET(digs_flamey_feud_hurt_them),
    DIGS_SET(digs_flamey_feud_killed_by),
    DIGS_SET(digs_flamey_feud_killed_them),
    DIGS_SET(digs_flamey_feud_revenge),
    DIGS_SET(digs_flamey_feud_spotted),
    DIGS_SET(digs_flamey_feud_taunted),
    DIGS_SET(digs_flamey_hostile_humiliated),
    DIGS_SET(digs_flamey_hostile_hurt_by),
    DIGS_SET(digs_flamey_hostile_hurt_them),
    DIGS_SET(digs_flamey_hostile_killed_by),
    DIGS_SET(digs_flamey_hostile_killed_them),
    DIGS_SET(digs_flamey_hostile_revenge),
    DIGS_SET(digs_flamey_hostile_spotted),
    DIGS_SET(digs_flamey_hostile_taunted),
    DIGS_SET(digs_flamey_needling_humiliated),
    DIGS_SET(digs_flamey_needling_hurt_by),
    DIGS_SET(digs_flamey_needling_hurt_them),
    DIGS_SET(digs_flamey_needling_killed_by),
    DIGS_SET(digs_flamey_needling_killed_them),
    DIGS_SET(digs_flamey_needling_revenge),
    DIGS_SET(digs_flamey_needling_spotted),
    DIGS_SET(digs_flamey_needling_taunted),
    DIGS_SET(digs_flamey_neutral_hurt_by),
    DIGS_SET(digs_flamey_neutral_hurt_them),
    DIGS_SET(digs_flamey_neutral_killed_by),
    DIGS_SET(digs_flamey_neutral_killed_them),
    DIGS_SET(digs_flamey_neutral_saved_by),
    DIGS_SET(digs_flamey_neutral_spotted),
    DIGS_SET(digs_flamey_neutral_teamed_up),
    DIGS_SET(digs_flamey_neutral_truce_broken),
    DIGS_SET(digs_flamey_thawing_hurt_by),
    DIGS_SET(digs_flamey_thawing_hurt_them),
    DIGS_SET(digs_flamey_thawing_killed_by),
    DIGS_SET(digs_flamey_thawing_killed_them),
    DIGS_SET(digs_flamey_thawing_saved_by),
    DIGS_SET(digs_flamey_thawing_spotted),
    DIGS_SET(digs_flamey_thawing_teamed_up),
    DIGS_SET(digs_flamey_thawing_truce_broken),
    DIGS_SET(digs_flamey_truce_hurt_by),
    DIGS_SET(digs_flamey_truce_hurt_them),
    DIGS_SET(digs_flamey_truce_killed_by),
    DIGS_SET(digs_flamey_truce_killed_them),
    DIGS_SET(digs_flamey_truce_saved_by),
    DIGS_SET(digs_flamey_truce_spotted),
    DIGS_SET(digs_flamey_truce_teamed_up),
    DIGS_SET(digs_flamey_truce_truce_broken),
    DIGS_SET(digs_flamey_wary_humiliated),
    DIGS_SET(digs_flamey_wary_hurt_by),
    DIGS_SET(digs_flamey_wary_hurt_them),
    DIGS_SET(digs_flamey_wary_killed_by),
    DIGS_SET(digs_flamey_wary_killed_them),
    DIGS_SET(digs_flamey_wary_revenge),
    DIGS_SET(digs_flamey_wary_spotted),
    DIGS_SET(digs_flamey_wary_taunted),
    DIGS_SET(digs_miner_bonded_hurt_by),
    DIGS_SET(digs_miner_bonded_hurt_them),
    DIGS_SET(digs_miner_bonded_killed_by),
    DIGS_SET(digs_miner_bonded_killed_them),
    DIGS_SET(digs_miner_bonded_saved_by),
    DIGS_SET(digs_miner_bonded_spotted),
    DIGS_SET(digs_miner_bonded_teamed_up),
    DIGS_SET(digs_miner_bonded_truce_broken),
    DIGS_SET(digs_miner_feud_humiliated),
    DIGS_SET(digs_miner_feud_hurt_by),
    DIGS_SET(digs_miner_feud_hurt_them),
    DIGS_SET(digs_miner_feud_killed_by),
    DIGS_SET(digs_miner_feud_killed_them),
    DIGS_SET(digs_miner_feud_revenge),
    DIGS_SET(digs_miner_feud_spotted),
    DIGS_SET(digs_miner_feud_taunted),
    DIGS_SET(digs_miner_hostile_humiliated),
    DIGS_SET(digs_miner_hostile_hurt_by),
    DIGS_SET(digs_miner_hostile_hurt_them),
    DIGS_SET(digs_miner_hostile_killed_by),
    DIGS_SET(digs_miner_hostile_killed_them),
    DIGS_SET(digs_miner_hostile_revenge),
    DIGS_SET(digs_miner_hostile_spotted),
    DIGS_SET(digs_miner_hostile_taunted),
    DIGS_SET(digs_miner_needling_humiliated),
    DIGS_SET(digs_miner_needling_hurt_by),
    DIGS_SET(digs_miner_needling_hurt_them),
    DIGS_SET(digs_miner_needling_killed_by),
    DIGS_SET(digs_miner_needling_killed_them),
    DIGS_SET(digs_miner_needling_revenge),
    DIGS_SET(digs_miner_needling_spotted),
    DIGS_SET(digs_miner_needling_taunted),
    DIGS_SET(digs_miner_neutral_hurt_by),
    DIGS_SET(digs_miner_neutral_hurt_them),
    DIGS_SET(digs_miner_neutral_killed_by),
    DIGS_SET(digs_miner_neutral_killed_them),
    DIGS_SET(digs_miner_neutral_saved_by),
    DIGS_SET(digs_miner_neutral_spotted),
    DIGS_SET(digs_miner_neutral_teamed_up),
    DIGS_SET(digs_miner_neutral_truce_broken),
    DIGS_SET(digs_miner_thawing_hurt_by),
    DIGS_SET(digs_miner_thawing_hurt_them),
    DIGS_SET(digs_miner_thawing_killed_by),
    DIGS_SET(digs_miner_thawing_killed_them),
    DIGS_SET(digs_miner_thawing_saved_by),
    DIGS_SET(digs_miner_thawing_spotted),
    DIGS_SET(digs_miner_thawing_teamed_up),
    DIGS_SET(digs_miner_thawing_truce_broken),
    DIGS_SET(digs_miner_truce_hurt_by),
    DIGS_SET(digs_miner_truce_hurt_them),
    DIGS_SET(digs_miner_truce_killed_by),
    DIGS_SET(digs_miner_truce_killed_them),
    DIGS_SET(digs_miner_truce_saved_by),
    DIGS_SET(digs_miner_truce_spotted),
    DIGS_SET(digs_miner_truce_teamed_up),
    DIGS_SET(digs_miner_truce_truce_broken),
    DIGS_SET(digs_miner_wary_humiliated),
    DIGS_SET(digs_miner_wary_hurt_by),
    DIGS_SET(digs_miner_wary_hurt_them),
    DIGS_SET(digs_miner_wary_killed_by),
    DIGS_SET(digs_miner_wary_killed_them),
    DIGS_SET(digs_miner_wary_revenge),
    DIGS_SET(digs_miner_wary_spotted),
    DIGS_SET(digs_miner_wary_taunted),
    DIGS_SET(digs_rivet_bonded_hurt_by),
    DIGS_SET(digs_rivet_bonded_hurt_them),
    DIGS_SET(digs_rivet_bonded_killed_by),
    DIGS_SET(digs_rivet_bonded_killed_them),
    DIGS_SET(digs_rivet_bonded_saved_by),
    DIGS_SET(digs_rivet_bonded_spotted),
    DIGS_SET(digs_rivet_bonded_teamed_up),
    DIGS_SET(digs_rivet_bonded_truce_broken),
    DIGS_SET(digs_rivet_feud_humiliated),
    DIGS_SET(digs_rivet_feud_hurt_by),
    DIGS_SET(digs_rivet_feud_hurt_them),
    DIGS_SET(digs_rivet_feud_killed_by),
    DIGS_SET(digs_rivet_feud_killed_them),
    DIGS_SET(digs_rivet_feud_revenge),
    DIGS_SET(digs_rivet_feud_spotted),
    DIGS_SET(digs_rivet_feud_taunted),
    DIGS_SET(digs_rivet_hostile_humiliated),
    DIGS_SET(digs_rivet_hostile_hurt_by),
    DIGS_SET(digs_rivet_hostile_hurt_them),
    DIGS_SET(digs_rivet_hostile_killed_by),
    DIGS_SET(digs_rivet_hostile_killed_them),
    DIGS_SET(digs_rivet_hostile_revenge),
    DIGS_SET(digs_rivet_hostile_spotted),
    DIGS_SET(digs_rivet_hostile_taunted),
    DIGS_SET(digs_rivet_needling_humiliated),
    DIGS_SET(digs_rivet_needling_hurt_by),
    DIGS_SET(digs_rivet_needling_hurt_them),
    DIGS_SET(digs_rivet_needling_killed_by),
    DIGS_SET(digs_rivet_needling_killed_them),
    DIGS_SET(digs_rivet_needling_revenge),
    DIGS_SET(digs_rivet_needling_spotted),
    DIGS_SET(digs_rivet_needling_taunted),
    DIGS_SET(digs_rivet_neutral_hurt_by),
    DIGS_SET(digs_rivet_neutral_hurt_them),
    DIGS_SET(digs_rivet_neutral_killed_by),
    DIGS_SET(digs_rivet_neutral_killed_them),
    DIGS_SET(digs_rivet_neutral_saved_by),
    DIGS_SET(digs_rivet_neutral_spotted),
    DIGS_SET(digs_rivet_neutral_teamed_up),
    DIGS_SET(digs_rivet_neutral_truce_broken),
    DIGS_SET(digs_rivet_thawing_hurt_by),
    DIGS_SET(digs_rivet_thawing_hurt_them),
    DIGS_SET(digs_rivet_thawing_killed_by),
    DIGS_SET(digs_rivet_thawing_killed_them),
    DIGS_SET(digs_rivet_thawing_saved_by),
    DIGS_SET(digs_rivet_thawing_spotted),
    DIGS_SET(digs_rivet_thawing_teamed_up),
    DIGS_SET(digs_rivet_thawing_truce_broken),
    DIGS_SET(digs_rivet_truce_hurt_by),
    DIGS_SET(digs_rivet_truce_hurt_them),
    DIGS_SET(digs_rivet_truce_killed_by),
    DIGS_SET(digs_rivet_truce_killed_them),
    DIGS_SET(digs_rivet_truce_saved_by),
    DIGS_SET(digs_rivet_truce_spotted),
    DIGS_SET(digs_rivet_truce_teamed_up),
    DIGS_SET(digs_rivet_truce_truce_broken),
    DIGS_SET(digs_rivet_wary_humiliated),
    DIGS_SET(digs_rivet_wary_hurt_by),
    DIGS_SET(digs_rivet_wary_hurt_them),
    DIGS_SET(digs_rivet_wary_killed_by),
    DIGS_SET(digs_rivet_wary_killed_them),
    DIGS_SET(digs_rivet_wary_revenge),
    DIGS_SET(digs_rivet_wary_spotted),
    DIGS_SET(digs_rivet_wary_taunted),
    DIGS_SET(digs_cinder_feud_truce_accepted),
    DIGS_SET(digs_cinder_feud_truce_offered),
    DIGS_SET(digs_cinder_neutral_truce_accepted),
    DIGS_SET(digs_cinder_neutral_truce_offered),
    DIGS_SET(digs_cinder_thawing_truce_accepted),
    DIGS_SET(digs_cinder_thawing_truce_offered),
    DIGS_SET(digs_cinder_truce_truce_accepted),
    DIGS_SET(digs_cinder_truce_truce_offered),
    DIGS_SET(digs_flamey_feud_truce_accepted),
    DIGS_SET(digs_flamey_feud_truce_offered),
    DIGS_SET(digs_flamey_neutral_truce_accepted),
    DIGS_SET(digs_flamey_neutral_truce_offered),
    DIGS_SET(digs_flamey_thawing_truce_accepted),
    DIGS_SET(digs_flamey_thawing_truce_offered),
    DIGS_SET(digs_flamey_truce_truce_accepted),
    DIGS_SET(digs_flamey_truce_truce_offered),
    DIGS_SET(digs_miner_feud_truce_accepted),
    DIGS_SET(digs_miner_feud_truce_offered),
    DIGS_SET(digs_miner_neutral_truce_accepted),
    DIGS_SET(digs_miner_neutral_truce_offered),
    DIGS_SET(digs_miner_thawing_truce_accepted),
    DIGS_SET(digs_miner_thawing_truce_offered),
    DIGS_SET(digs_miner_truce_truce_accepted),
    DIGS_SET(digs_miner_truce_truce_offered),
    DIGS_SET(digs_rivet_feud_truce_accepted),
    DIGS_SET(digs_rivet_feud_truce_offered),
    DIGS_SET(digs_rivet_neutral_truce_accepted),
    DIGS_SET(digs_rivet_neutral_truce_offered),
    DIGS_SET(digs_rivet_thawing_truce_accepted),
    DIGS_SET(digs_rivet_thawing_truce_offered),
    DIGS_SET(digs_rivet_truce_truce_accepted),
    DIGS_SET(digs_rivet_truce_truce_offered),

    /* Contextual bark fallback pools: 369 through 378. */
    DIGS_SET(digs_any_move),
    DIGS_SET(digs_any_weapon),
    DIGS_SET(digs_any_miss),
    DIGS_SET(digs_any_hit),
    DIGS_SET(digs_any_near_death),
    DIGS_SET(digs_any_cave_in),
    DIGS_SET(digs_any_grapple),
    DIGS_SET(digs_any_kill),
    DIGS_SET(digs_any_humiliation),
    DIGS_SET(digs_any_escape),

    /* Authored player pools: 379 through 388. */
    DIGS_SET(digs_miner_move),
    DIGS_SET(digs_miner_weapon),
    DIGS_SET(digs_miner_miss),
    DIGS_SET(digs_miner_hit),
    DIGS_SET(digs_miner_near_death),
    DIGS_SET(digs_miner_cave_in),
    DIGS_SET(digs_miner_grapple),
    DIGS_SET(digs_miner_kill),
    DIGS_SET(digs_miner_humiliation),
    DIGS_SET(digs_miner_escape)
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
     0U, 0U, 0U, 0U, 37U, 38U, 39U, 40U, 0U, 41U, 0U, 42U,
     0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U},
    /* CINDER */
    {0U, 43U, 44U, 45U, 46U, 47U, 48U, 49U, 50U, 51U, 52U, 0U, 0U, 0U, 0U,
     0U, 0U, 0U, 0U, 53U, 54U, 55U, 56U, 0U, 57U, 0U, 58U,
     0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U},
    /* FLAMEY */
    {0U, 59U, 60U, 61U, 62U, 63U, 64U, 65U, 66U, 67U, 68U, 0U, 0U, 0U, 0U,
     0U, 0U, 0U, 0U, 69U, 70U, 71U, 72U, 0U, 73U, 0U, 74U,
     0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U},
    /* the miner */
    {0U, 75U, 0U, 76U, 77U, 0U, 0U, 0U, 78U, 79U, 0U, 0U, 0U, 0U, 0U,
     0U, 0U, 0U, 0U, 0U, 0U, 0U, 80U, 0U, 0U, 0U, 0U,
     379U, 380U, 381U, 382U, 383U, 384U, 385U, 386U, 387U, 388U}
};

/* The floor: what anyone would say.  No entry here may be zero. */
static const vox_u16 digs_any_sets[VOX_DIGS_STIMULUS_COUNT] = {
    0U,     /* NONE has nothing to say, by definition */
    1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U,
    16U, 17U, 18U, 19U, 20U, 21U, 22U, 23U, 24U, 25U, 26U,
    369U, 370U, 371U, 372U, 373U, 374U, 375U, 376U, 377U, 378U
};

/*
 * Which set a specific (voice, tone, stimulus) resolves to.  Zero means
 * nobody wrote that combination and the voice pool below it answers instead.
 */
static const vox_u16 digs_tone_sets[DIGS_VOICE_COUNT][VOX_DIGS_TONE_COUNT]
                                   [VOX_DIGS_STIMULUS_COUNT] = {
    {   /* RIVET */
        {   /* FEUD */
            0U, 0U, 287U, 283U, 282U, 0U, 0U, 0U, 285U, 284U, 286U, 281U,
            0U, 0U, 0U, 0U, 362U, 361U, 0U, 288U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U
        },
        {   /* HOSTILE */
            0U, 0U, 295U, 291U, 290U, 0U, 0U, 0U, 293U, 292U, 294U, 289U,
            0U, 0U, 0U, 0U, 0U, 0U, 0U, 296U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        },
        {   /* NEEDLING */
            0U, 0U, 303U, 299U, 298U, 0U, 0U, 0U, 301U, 300U, 302U, 297U,
            0U, 0U, 0U, 0U, 0U, 0U, 0U, 304U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        },
        {   /* NEUTRAL */
            0U, 0U, 310U, 306U, 305U, 0U, 0U, 0U, 308U, 307U, 0U, 0U, 0U,
            309U, 311U, 0U, 364U, 363U, 312U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U
        },
        {   /* WARY */
            0U, 0U, 335U, 331U, 330U, 0U, 0U, 0U, 333U, 332U, 334U, 329U,
            0U, 0U, 0U, 0U, 0U, 0U, 0U, 336U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        },
        {   /* THAWING */
            0U, 0U, 318U, 314U, 313U, 0U, 0U, 0U, 316U, 315U, 0U, 0U, 0U,
            317U, 319U, 0U, 366U, 365U, 320U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U
        },
        {   /* TRUCE */
            0U, 0U, 326U, 322U, 321U, 0U, 0U, 0U, 324U, 323U, 0U, 0U, 0U,
            325U, 327U, 0U, 368U, 367U, 328U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U
        },
        {   /* BONDED */
            0U, 0U, 278U, 274U, 273U, 0U, 0U, 0U, 276U, 275U, 0U, 0U, 0U,
            277U, 279U, 0U, 0U, 0U, 280U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        }
    },
    {   /* CINDER */
        {   /* FEUD */
            0U, 0U, 95U, 91U, 90U, 0U, 0U, 0U, 93U, 92U, 94U, 89U, 0U, 0U,
            0U, 0U, 338U, 337U, 0U, 96U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        },
        {   /* HOSTILE */
            0U, 0U, 103U, 99U, 98U, 0U, 0U, 0U, 101U, 100U, 102U, 97U, 0U,
            0U, 0U, 0U, 0U, 0U, 0U, 104U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        },
        {   /* NEEDLING */
            0U, 0U, 111U, 107U, 106U, 0U, 0U, 0U, 109U, 108U, 110U, 105U,
            0U, 0U, 0U, 0U, 0U, 0U, 0U, 112U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        },
        {   /* NEUTRAL */
            0U, 0U, 118U, 114U, 113U, 0U, 0U, 0U, 116U, 115U, 0U, 0U, 0U,
            117U, 119U, 0U, 340U, 339U, 120U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U
        },
        {   /* WARY */
            0U, 0U, 143U, 139U, 138U, 0U, 0U, 0U, 141U, 140U, 142U, 137U,
            0U, 0U, 0U, 0U, 0U, 0U, 0U, 144U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        },
        {   /* THAWING */
            0U, 0U, 126U, 122U, 121U, 0U, 0U, 0U, 124U, 123U, 0U, 0U, 0U,
            125U, 127U, 0U, 342U, 341U, 128U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U
        },
        {   /* TRUCE */
            0U, 0U, 134U, 130U, 129U, 0U, 0U, 0U, 132U, 131U, 0U, 0U, 0U,
            133U, 135U, 0U, 344U, 343U, 136U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U
        },
        {   /* BONDED */
            0U, 0U, 86U, 82U, 81U, 0U, 0U, 0U, 84U, 83U, 0U, 0U, 0U, 85U,
            87U, 0U, 0U, 0U, 88U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        }
    },
    {   /* FLAMEY */
        {   /* FEUD */
            0U, 0U, 159U, 155U, 154U, 0U, 0U, 0U, 157U, 156U, 158U, 153U,
            0U, 0U, 0U, 0U, 346U, 345U, 0U, 160U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U
        },
        {   /* HOSTILE */
            0U, 0U, 167U, 163U, 162U, 0U, 0U, 0U, 165U, 164U, 166U, 161U,
            0U, 0U, 0U, 0U, 0U, 0U, 0U, 168U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        },
        {   /* NEEDLING */
            0U, 0U, 175U, 171U, 170U, 0U, 0U, 0U, 173U, 172U, 174U, 169U,
            0U, 0U, 0U, 0U, 0U, 0U, 0U, 176U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        },
        {   /* NEUTRAL */
            0U, 0U, 182U, 178U, 177U, 0U, 0U, 0U, 180U, 179U, 0U, 0U, 0U,
            181U, 183U, 0U, 348U, 347U, 184U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U
        },
        {   /* WARY */
            0U, 0U, 207U, 203U, 202U, 0U, 0U, 0U, 205U, 204U, 206U, 201U,
            0U, 0U, 0U, 0U, 0U, 0U, 0U, 208U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        },
        {   /* THAWING */
            0U, 0U, 190U, 186U, 185U, 0U, 0U, 0U, 188U, 187U, 0U, 0U, 0U,
            189U, 191U, 0U, 350U, 349U, 192U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U
        },
        {   /* TRUCE */
            0U, 0U, 198U, 194U, 193U, 0U, 0U, 0U, 196U, 195U, 0U, 0U, 0U,
            197U, 199U, 0U, 352U, 351U, 200U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U
        },
        {   /* BONDED */
            0U, 0U, 150U, 146U, 145U, 0U, 0U, 0U, 148U, 147U, 0U, 0U, 0U,
            149U, 151U, 0U, 0U, 0U, 152U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        }
    },
    {   /* MINER */
        {   /* FEUD */
            0U, 0U, 223U, 219U, 218U, 0U, 0U, 0U, 221U, 220U, 222U, 217U,
            0U, 0U, 0U, 0U, 354U, 353U, 0U, 224U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U
        },
        {   /* HOSTILE */
            0U, 0U, 231U, 227U, 226U, 0U, 0U, 0U, 229U, 228U, 230U, 225U,
            0U, 0U, 0U, 0U, 0U, 0U, 0U, 232U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        },
        {   /* NEEDLING */
            0U, 0U, 239U, 235U, 234U, 0U, 0U, 0U, 237U, 236U, 238U, 233U,
            0U, 0U, 0U, 0U, 0U, 0U, 0U, 240U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        },
        {   /* NEUTRAL */
            0U, 0U, 246U, 242U, 241U, 0U, 0U, 0U, 244U, 243U, 0U, 0U, 0U,
            245U, 247U, 0U, 356U, 355U, 248U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U
        },
        {   /* WARY */
            0U, 0U, 271U, 267U, 266U, 0U, 0U, 0U, 269U, 268U, 270U, 265U,
            0U, 0U, 0U, 0U, 0U, 0U, 0U, 272U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        },
        {   /* THAWING */
            0U, 0U, 254U, 250U, 249U, 0U, 0U, 0U, 252U, 251U, 0U, 0U, 0U,
            253U, 255U, 0U, 358U, 357U, 256U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U
        },
        {   /* TRUCE */
            0U, 0U, 262U, 258U, 257U, 0U, 0U, 0U, 260U, 259U, 0U, 0U, 0U,
            261U, 263U, 0U, 360U, 359U, 264U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U
        },
        {   /* BONDED */
            0U, 0U, 214U, 210U, 209U, 0U, 0U, 0U, 212U, 211U, 0U, 0U, 0U,
            213U, 215U, 0U, 0U, 0U, 216U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
        }
    }
};


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

/*
 * Every set must fit the stride, and every id must fit a vox_u16.  This is
 * the check that turns the packing constraint from a comment into a test.
 */
int digs_lines_stride_is_sound(void)
{
    vox_u32 set;
    for (set = 0U; set < DIGS_LINE_SET_COUNT; ++set) {
        if (digs_line_sets[set].count > DIGS_LINE_SET_STRIDE) {
            return 0;
        }
    }
    return (DIGS_LINE_SET_COUNT * DIGS_LINE_SET_STRIDE) <= 65535U;
}

int digs_lines_addresses(vox_u16 id)
{
    const char *text = digs_lines_text(id);
    vox_u16 i;
    for (i = 0U; text[i] != '\0' && i < 255U; ++i) {
        if (text[i] == '%' && text[i + 1] == 'T') {
            return 1;
        }
    }
    return 0;
}

vox_u16 digs_lines_length(vox_u16 id)
{
    const char *text = digs_lines_text(id);
    vox_u16 count = 0U;
    while (text[count] != '\0' && count < 255U) {
        count++;
    }
    return count;
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

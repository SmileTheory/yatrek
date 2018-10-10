#ifdef __cplusplus
#include <cassert>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#else
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#endif

#define INLINE static inline

////////////////////////////////////////////////////////////////////////////////
// below from "A small noncryptographic PRNG"
// http://burtleburtle.net/bob/rand/smallprng.html

typedef uint32_t u4;
typedef struct ranctx { u4 a; u4 b; u4 c; u4 d; } ranctx;

#define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))
u4 ranval(ranctx *x)
{
	u4 e = x->a - rot(x->b, 27);

	x->a = x->b ^ rot(x->c, 17);
	x->b = x->c + x->d;
	x->c = x->d + e;
	x->d = e + x->a;
	return x->d;
}

INLINE void raninit(ranctx *x, u4 seed)
{
	u4 i;

	x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
	for (i = 0; i < 20; ++i)
		(void)ranval(x);
}

////////////////////////////////////////////////////////////////////////////////

ranctx rstate;

INLINE float b_RND(double R)
{
	u4 val;

	if (R == 0)
		val = rstate.d;
	else
		val = ranval(&rstate);

	return (val >> 8) * (1. / (1 << 24));
}

const char *b_SPC(int len)
{
	const char *space = "                                                                                ";

	if (len > 80 || len < 0)
		return NULL;

	return space + 80 - len;
}

INLINE void input80(char *s, size_t size)
{
	fflush(stdout);
	fgets(s, size, stdin);
	s[size-1] = '\0';
}

INLINE void b_INPUT_1(double *num)
{
	char s[80];
	input80(s, sizeof(s));
	sscanf(s, "%lf", num);
}

INLINE void b_INPUT_2(double *num1, double *num2)
{
	char s[80];
	input80(s, sizeof(s));
	sscanf(s, "%lf,%lf", num1, num2);
}

INLINE void b_INPUT_S(char *out, int size)
{
	char s[80];
	int i;

	input80(s, sizeof(s));

	for (i = 0; i < size; i++)
	{
		if (s[i] == '\n') {
			out[i] = '\0';
			break;
		} else if (s[i] == '\0')
			break;
		out[i] = s[i];
	}
	out[size - 1]  = '\0';
}

typedef struct {
	int X;
	int Y;
} ivec2;

typedef struct {
	double X;
	double Y;
} vec2;

#define VEC2_COPY(r, a) ((r).X = (a).X, (r).Y = (a).Y)
#define VEC2_SET(r, x, y) ((r).X = (x), (r).Y = (y))
#define VEC2_ADD(r, a, b) ((r).X = (a).X + (b).X, (r).Y = (a).Y + (b).Y)
#define VEC2_SUB(r, a, b) ((r).X = (a).X - (b).X, (r).Y = (a).Y - (b).Y)
#define VEC2_MAD(r, a, s, b) ((r).X = (a).X + (s) * (b).X, (r).Y = (a).Y + (s) * (b).Y)

/*
 * int variables notes:
 *
 * E can be non-int, because there is no check for firing phasers at non-int
 * strength, and there is no check for setting shield strength to non-int.
 *
 * (If energy transfer from shields to warp drive is enabled)
 * S can go non-int because E can be non-int and insufficient to perform a
 * maneuver, and then added to S.
 *
 * S9, E0 are technically always int, but are basically constants so have
 * been left as double for flexibility.
 *
 */

int    G[9][9],  // galaxy info: 100 * klingons + 10 * starbases + stars
       Z[9][9];  // known galaxy info, same format as G[][]

struct {
	ivec2 XY[4];
	double E[4];
} K;   // klingon info

double D[9];     // devices state of repair, < 0 -> damaged

double T,  // current stardate
       T0, // initial stardate
       T9, // remaining stardates

       E,  // current energy
       E0, // max energy
       S,  // shield strength

       S9, // average klingon energy

       D4; // random extra starbase repair time

ivec2 Q_12, // enterprise quadrant
      S_12, // enterprise sector
      B_45; // starbase sector

int    P,  // current # of torpedoes
       P0, // max # of torpedoes

       K3, // # of klingons in quadrant
       K7, // starting # of klingons in galaxy
       K9, // current # of klingons in galaxy

       B3, // # of starbases in quadrant
       B9, // current # of starbases in galaxy

       S3; // # of stars in quadrant

bool D0; // flag - docked

// list of commands
const char *s_A1 = "NAVSRSLRSPHATORSHEDAMCOMXXX";

char s_Q[192];  // quadrant string

INLINE double b_FND(int I)
{
	ivec2 XY;
	VEC2_SUB(XY, K.XY[I], S_12);
	return sqrt(XY.X * XY.X + XY.Y * XY.Y);
}

int b_FNR()
{
	return b_RND(1) * 7.98 + 1.01;
}

int my_strnicmp(const char *a, const char *b, size_t len)
{
	while (len--)
	{
		int v = toupper(*a) - toupper(*b);

		if (v || !*a)
			return v;

		a++;
		b++;
	}

	return 0;
}

typedef enum {
	GALAXY_MAP_NAMES,
	GALAXY_MAP_RECORD,
} gm_t;

typedef enum {
	RG_PASS = 0,  // no end, continue
	RG_MAIN_LOOP, // return to main loop
	RG_GAME_END_TIME,
	RG_GAME_END_NO_ENERGY,
	RG_GAME_END_ENTERPRISE_DESTROYED,
	RG_GAME_END_RESIGN,
	RG_GAME_END_TORPEDOED_STARBASE,
	RG_GAME_END_NO_KLINGONS,
} rg_t;

enum {
	DEVICE_WARP_ENGINES = 1,
	DEVICE_SHORT_RANGE_SENSORS,
	DEVICE_LONG_RANGE_SENSORS,
	DEVICE_PHASER_CONTROL,
	DEVICE_PHOTON_TUBES,
	DEVICE_DAMAGE_CONTROL,
	DEVICE_SHIELD_CONTROL,
	DEVICE_LIBRARY_COMPUTER
};

INLINE void new_galaxy();
void new_quadrant();
INLINE void main_loop();
INLINE rg_t course_control();
INLINE rg_t exceeded_quadrant_limits(int N, vec2 X_12);
void maneuver_energy(int N);
INLINE void long_range_sensors();
INLINE rg_t phaser_control();
INLINE rg_t photon_torpedo();
INLINE void shield_control();
INLINE void damage_control();
rg_t klingons_shooting();
INLINE void end_of_game(rg_t end_type);
void short_range_sensors_dock();
INLINE void library_computer();
INLINE void galaxy_map(gm_t map_type);
INLINE void status_report();
INLINE void dir_calc_klingons();
INLINE void dir_calc_starbase();
INLINE void dir_calc_input();
INLINE ivec2 set_empty_sector(const char *s_A);
INLINE void set_sector(ivec2 Z_12, const char *s_A);
const char *get_device_name(int R1);
INLINE bool is_sector(ivec2 Z_12, const char *s_A);
const char *get_quadrant_name(int Z4, int Z5);
INLINE const char *get_quadrant_number(int Z5);

INLINE vec2 course_to_delta(double C1)
{
	// course calc constants
	const signed char C[9] = { 0, -1, -1, -1, 0,  1,  1,  1, 0 };
	vec2 X_12;
	int C2 = C1;

	C1 -= C2;
	C2--;
	X_12.X = C[C2] + (C[C2 + 1] - C[C2]) * C1;

	C2 = (C2 + 6) & 7;
	X_12.Y = C[C2] + (C[C2 + 1] - C[C2]) * C1;

	return X_12;
}

INLINE void linefeeds(int num)
{
	while (num--)
		putchar('\n');
}

INLINE void instructions()
{
	const char *inst_txt =
"\n"
"      INSTRUCTIONS FOR 'SUPER STAR TREK'\n"
"\n"
"1. WHEN YOU SEE \\COMMAND ?\\ PRINTED, ENTER ONE OF THE LEGAL\n"
"     COMMANDS (NAV,SRS,LRS,PHA,TOR,SHE,DAM,COM, OR XXX).\n"
"2. IF YOU SHOULD TYPE IN AN ILLEGAL COMMAND, YOU'LL GET A SHORT\n"
"     LIST OF THE LEGAL COMMANDS PRINTED OUT.\n"
"3. SOME COMMANDS REQUIRE YOU TO ENTER DATA (FOR EXAMPLE, THE\n"
"     'NAV' COMMAND COMES BACK WITH 'COURSE (1-9) ?'.)  IF YOU\n"
"     TYPE IN ILLEGAL DATA (LIKE NEGATIVE NUMBERS), THAT COMMAND\n"
"     WILL BE ABORTED\n"
"\n"
"     THE GALAXY IS DIVIDED INTO AN 8 X 8 QUADRANT GRID,\n"
"AND EACH QUADRANT IS FURTHER DIVIDED INTO AN 8 X 8 SECTOR GRID.\n"
"\n"
"     YOU WILL BE ASSIGNED A STARTING POINT SOMEWHERE IN THE\n"
"GALAXY TO BEGIN A TOUR OF DUTY AS COMMANDER OF THE STARSHIP\n"
"\\ENTERPRISE\\; YOUR MISSION: TO SEEK AND DESTROY THE FLEET OF\n"
"KLINGON WARWHIPS WHICH ARE MENACING THE UNITED FEDERATION OF\n"
"PLANETS.\n"
"\n"
"$"
"     YOU HAVE THE FOLLOWING COMMANDS AVAILABLE TO YOU AS CAPTAIN\n"
"OF THE STARSHIP ENTERPRISE:\n"
"\n"
"\\NAV\\ COMMAND = WARP ENGINE CONTROL --\n"
"     COURSE IS IN A CIRCULAR NUMERICAL      4  3  2\n"
"     VECTOR ARRANGEMENT AS SHOWN             . . .\n"
"     INTEGER AND REAL VALUES MAY BE           ...\n"
"     USED.  (THUS COURSE 1.5 IS HALF-     5 ---*--- 1\n"
"     WAY BETWEEN 1 AND 2                      ...\n"
"                                             . . .\n"
"     VALUES MAY APPROACH 9.0, WHICH         6  7  8\n"
"     ITSELF IS EQUIVALENT TO 1.0\n"
"                                            COURSE\n"
"     ONE WARP FACTOR IS THE SIZE OF\n"
"     ONE QUADTANT.  THEREFORE, TO GET\n"
"     FROM QUADRANT 6,5 TO 5,5, YOU WOULD\n"
"     USE COURSE 3, WARP FACTOR 1.\n"
"\n"
"$"
"\\SRS\\ COMMAND = SHORT RANGE SENSOR SCAN\n"
"     SHOWS YOU A SCAN OF YOUR PRESENT QUADRANT.\n"
"\n"
"     SYMBOLOGY ON YOUR SENSOR SCREEN IS AS FOLLOWS:\n"
"        <*> = YOUR STARSHIP'S POSITION\n"
"        +K+ = KLINGON BATTLE CRUISER\n"
"        >!< = FEDERATION STARBASE (REFUEL/REPAIR/RE-ARM HERE!)\n"
"         *  = STAR\n"
"\n"
"     A CONDENSED 'STATUS REPORT' WILL ALSO BE PRESENTED.\n"
"\n"
"\\LRS\\ COMMAND = LONG RANGE SENSOR SCAN\n"
"     SHOWS CONDITIONS IN SPACE FOR ONE QUADRANT ON EACH SIDE\n"
"     OF THE ENTERPRISE (WHICH IS IN THE MIDDLE OF THE SCAN)\n"
"     THE SCAN IS CODED IN THE FORM \\###\\, WHERE TH UNITS DIGIT\n"
"     IS THE NUMBER OF STARS, THE TENS DIGIT IS THE NUMBER OF\n"
"     STARBASES, AND THE HUNDRESDS DIGIT IS THE NUMBER OF\n"
"     KLINGONS.\n"
"\n"
"     EXAMPLE - 207 = 2 KLINGONS, NO STARBASES, & 7 STARS.\n"
"\n"
"$"
"\\PHA\\ COMMAND = PHASER CONTROL.\n"
"     ALLOWS YOU TO DESTROY THE KLINGON BATTLE CRUISERS BY\n"
"     ZAPPING THEM WITH SUITABLY LARGE UNITS OF ENERGY TO\n"
"     DEPLETE THEIR SHIELD POWER.  (REMBER, KLINGONS HAVE\n"
"     PHASERS TOO!)\n"
"\n"
"\\TOR\\ COMMAND = PHOTON TORPEDO CONTROL\n"
"     TORPEDO COURSE IS THE SAME AS USED IN WARP ENGINE CONTROL\n"
"     IF YOU HIT THE KLINGON VESSEL, HE IS DESTROYED AND\n"
"     CANNOT FIRE BACK AT YOU.  IF YOU MISS, YOU ARE SUBJECT TO\n"
"     HIS PHASER FIRE.  IN EITHER CASE, YOU ARE ALSO SUBJECT TO\n"
"     THE PHASER FIRE OF ALL OTHER KLINGONS IN THE QUADRANT.\n"
"\n"
"     THE LIBRARY-COMPUTER (\\COM\\ COMMAND) HAS AN OPTION TO\n"
"     COMPUTE TORPEDO TRAJECTORY FOR YOU (OPTION 2)\n"
"\n"
"$"
"\\SHE\\ COMMAND = SHIELD CONTROL\n"
"     DEFINES THE NUMBER OF ENERGY UNITS TO BE ASSIGNED TO THE\n"
"     SHIELDS.  ENERGY IS TAKEN FROM TOTAL SHIP'S ENERGY.  NOTE\n"
"     THAN THE STATUS DISPLAY TOTAL ENERGY INCLUDES SHIELD ENERGY\n"
"\n"
"\\DAM\\ COMMAND = DAMMAGE CONTROL REPORT\n"
"     GIVES THE STATE OF REPAIR OF ALL DEVICES.  WHERE A NEGATIVE\n"
"     'STATE OF REPAIR' SHOWS THAT THE DEVICE IS TEMPORARILY\n"
"     DAMAGED.\n"
"\n"
"$"
"\\COM\\ COMMAND = LIBRARY-COMPUTER\n"
"     THE LIBRARY-COMPUTER CONTAINS SIX OPTIONS:\n"
"     OPTION 0 = CUMULATIVE GALACTIC RECORD\n"
"        THIS OPTION SHOWES COMPUTER MEMORY OF THE RESULTS OF ALL\n"
"        PREVIOUS SHORT AND LONG RANGE SENSOR SCANS\n"
"     OPTION 1 = STATUS REPORT\n"
"        THIS OPTION SHOWS THE NUMBER OF KLINGONS, STARDATES,\n"
"        AND STARBASES REMAINING IN THE GAME.\n"
"     OPTION 2 = PHOTON TORPEDO DATA\n"
"        WHICH GIVES DIRECTIONS AND DISTANCE FROM THE ENTERPRISE\n"
"        TO ALL KLINGONS IN YOUR QUADRANT\n"
"     OPTION 3 = STARBASE NAV DATA\n"
"        THIS OPTION GIVES DIRECTION AND DISTANCE TO ANY\n"
"        STARBASE WITHIN YOUR QUADRANT\n"
"     OPTION 4 = DIRECTION/DISTANCE CALCULATOR\n"
"        THIS OPTION ALLOWS YOU TO ENTER COORDINATES FOR\n"
"        DIRECTION/DISTANCE CALCULATIONS\n"
"     OPTION 5 = GALACTIC /REGION NAME/ MAP\n"
"        THIS OPTION PRINTS THE NAMES OF THE SIXTEEN MAJOR\n"
"        GALACTIC REGIONS REFERRED TO IN THE GAME.\n";

	char s_K[2];
	const char *p;

	linefeeds(12);
	printf("%s*************************************\n", b_SPC(10));
	printf("%s*                                   *\n", b_SPC(10));
	printf("%s*                                   *\n", b_SPC(10));
	printf("%s*      * * SUPER STAR TREK * *      *\n", b_SPC(10));
	printf("%s*                                   *\n", b_SPC(10));
	printf("%s*                                   *\n", b_SPC(10));
	printf("%s*************************************\n", b_SPC(10));
	linefeeds(8);

	printf("DO YOU NEED INSTRUCTIONS (Y/N)? ");
	b_INPUT_S(s_K, 2);

	if (my_strnicmp(s_K, "N", 1) == 0)
		return;

	p = inst_txt;
	do {
		int lines = 23;

		while (*p && *p != '$') {
			if (*p == '\n')
				lines--;
			putchar(*p++);
		}

		linefeeds(lines);
		printf("-PRESS ENTER TO CONTINUE-");
		b_INPUT_S(s_K, 2);
	} while(*p++);
}

int main(int argc, char *argv[])
{
	raninit(&rstate, argc == 2 ? atoi(argv[1]) : time(NULL));

	// SUPER STARTREK - MAY 16,1978 - REQUIRES 24K MEMORY
	//
	// ****        **** STAR TREK ****        ****
	// **** SIMULATION OF A MISSION OF THE STARSHIP ENTERPRISE,
	// **** AS SEEN ON THE STAR TREK TV SHOW.
	// **** ORIGIONAL PROGRAM BY MIKE MAYFIELD, MODIFIED VERSION
	// **** PUBLISHED IN DEC'S "101 BASIC GAMES", BY DAVE AHL.
	// **** MODIFICATIONS TO THE LATTER (PLUS DEBUGGING) BY BOB
	// *** LEEDOM - APRIL & DECEMBER 1974,
	// *** WITH A LITTLE HELP FROM HIS FRIENDS . . .
	// *** COMMENTS, EPITHETS, AND SUGGESTIONS SOLICITED --
	// *** SEND TO:  R. C. LEEDOM
	// ***           WESTINGHOUSE DEFENSE & ELECTRONICS SYSTEMS CNTR.
	// ***           BOX 746, M.S. 338
	// ***           BALTIMORE, MD  21203
	// ***
	// *** CONVERTED TO MICROSOFT 8 K BASIC 3/16/78 BY JOHN GORDERS
	// *** LINE NUMBERS FROM VERSION STREK7 OF 1/12/75 PRESERVED AS
	// *** MUCH AS POSSIBLE WHILE USING MULTIPLE STATEMENTS PER LINE
	// *** SOME LINES ARE LONGER THAN 72 CHARACTERS; THIS WAS DONE
	// *** BY USING "?" INSTEAD OF "PRINT" WHEN ENTERING LINES
	// ***

	instructions();

	while(1) {
		linefeeds(11);
		puts("                                    ,------*------,\n"
		     "                    ,-------------   '---  ------'\n"
		     "                     '-------- --'      / /\n"
		     "                         ,---' '-------/ /--,\n"
		     "                          '----------------'\n\n"
		     "                    THE USS ENTERPRISE --- NCC-1701");
		linefeeds(5);

		new_galaxy();
		new_quadrant();
		short_range_sensors_dock();
		main_loop();
	}
}


INLINE void new_galaxy()
{
	int I, J;
	const char *s_X, *s_X0; // plurals
	char s_A[3];

	T = (int)(b_RND(1) * 20 + 20) * 100;
	T0 = T;
	T9 = 25 + (int)(b_RND(1) * 10);
	D0 = false;
	E = 3000;
	E0 = E;
	P = 10;
	P0 = P;
	S9 = 200;
	S = 0;
	B9 = 0;
	K9 = 0;
	s_X = "";
	s_X0 = "IS";

	// INITIALIZE ENTERPRIZE'S POSITION
	Q_12.X = b_FNR(); Q_12.Y = b_FNR();
	S_12.X = b_FNR(); S_12.Y = b_FNR();

	for (I = 1; I <= 8; I++)
		D[I] = 0;

	// SETUP WHAT EXHISTS IN GALAXY . . .
	// K3= # KLINGONS  B3= # STARBASES  S3 = # STARS
	for (I = 1; I <= 8; I++) {
		for (J = 1; J <= 8; J++) {
			double dR1; // galaxy gen: random number

			Z[I][J] = 0;

			dR1 = b_RND(1);
			K3 = (dR1 > 0.98) ? 3 : (dR1 > 0.95) ? 2 : (dR1 > 0.8) ? 1 : 0;
			K9 += K3;

			B3 = (b_RND(1) > 0.96) ? 1 : 0;
			B9 += B3;

			G[I][J] = K3 * 100 + B3 * 10 + b_FNR();
		}
	}

	if (K9 > T9)
		T9 = K9 + 1;

	if (B9 == 0) {
		if (G[Q_12.X][Q_12.Y] < 200) {
			G[Q_12.X][Q_12.Y] += 100;
			K9++;
		}
		B9 = 1;
		G[Q_12.X][Q_12.Y] += 10;
		Q_12.X = b_FNR(); Q_12.Y = b_FNR();
	}

	K7 = K9;

	if (B9 != 1) {
		s_X = "S";
		s_X0 = "ARE";
	}

	printf("YOUR ORDERS ARE AS FOLLOWS:\n"
	       "     DESTROY THE %d KLINGON WARSHIPS WHICH HAVE INVADED\n"
	       "   THE GALAXY BEFORE THEY CAN ATTACK FEDERATION HEADQUARTERS\n"
	       "   ON STARDATE %g   THIS GIVES YOU %g DAYS.  THERE %s\n"
	       "   %d STARBASE%s IN THE GALAXY FOR RESUPPLYING YOUR SHIP\n"
	       "\nHIT ENTER WHEN READY TO ACCEPT COMMAND", K9, T0 + T9, T9, s_X0, B9, s_X);
	I = b_RND(1);
	b_INPUT_S(s_A, 3);
}


// HERE ANY TIME NEW QUADRANT ENTERED
void new_quadrant()
{
	int I;

	K3 = 0;
	B3 = 0;
	S3 = 0;
	D4 = 0.5 * b_RND(1);
	Z[Q_12.X][Q_12.Y] = G[Q_12.X][Q_12.Y];

	if (Q_12.X >= 1 && Q_12.X <= 8 && Q_12.Y >= 1 && Q_12.Y <= 8) {
		const char *s_G2_1 = get_quadrant_name(Q_12.X, Q_12.Y);
		const char *s_G2_2 = get_quadrant_number(Q_12.Y);

		linefeeds(1);
		if (T0 == T) {
			printf("YOUR MISSION BEGINS WITH YOUR STARSHIP LOCATED\n"
			       "IN THE GALACTIC QUADRANT, '%s %s'.\n", s_G2_1, s_G2_2);
		} else {
			printf("NOW ENTERING %s %s QUADRANT . . .\n", s_G2_1, s_G2_2);
		}
		linefeeds(1);

		K3 = G[Q_12.X][Q_12.Y] / 100;
		B3 = G[Q_12.X][Q_12.Y] / 10 % 10;
		S3 = G[Q_12.X][Q_12.Y] % 10;

		if (K3 != 0) {
			printf("COMBAT AREA      CONDITION RED\n");

			if (S <= 200)
				printf("   SHIELDS DANGEROUSLY LOW\n");
		}

		for (I = 1; I <= 3; I++) {
			VEC2_SET(K.XY[I], 0, 0);
		}
	}

	for (I = 1; I <= 3; I++) {
		K.E[I] = 0;
	}

	for (I = 0; I < 192; I++)
		s_Q[I] = ' ';

	// POSITION ENTERPRISE IN QUADRANT, THEN PLACE "K3" KLINGONS, &
	// "B3" STARBASES, & "S3" STARS ELSEWHERE.
	set_sector(S_12, "<*>");

	for (I = 1; I <= K3; I++) {
		K.XY[I] = set_empty_sector("+K+");
		K.E[I] = S9 * (0.5 + b_RND(1));
	}

	if (B3 >= 1) {
		B_45 = set_empty_sector(">!<");
	}

	for (I = 1; I <= S3; I++) {
		set_empty_sector(" * ");
	}
}


INLINE void main_loop()
{
	while(1) {
		char s_A[4];
		int I;
		rg_t ret;

		// BUG: game over triggered if shield control is overrepaired
		if (S + E <= 10 || (E <= 10 && D[DEVICE_SHIELD_CONTROL] != 0)) {
			end_of_game(RG_GAME_END_NO_ENERGY);
			return;
		}

		printf("COMMAND? ");
		b_INPUT_S(s_A, 4);

		for (I = 0; I < 9; I++) {
			if (my_strnicmp(s_A, s_A1 + 3 * I, 3) == 0)
				break;
		}

		ret = RG_MAIN_LOOP;

		switch(I)
		{
			case 0: ret = course_control(); break;
			case 1: short_range_sensors_dock(); break;
			case 2: long_range_sensors(); break;
			case 3: ret = phaser_control(); break;
			case 4: ret = photon_torpedo(); break;
			case 5: shield_control(); break;
			case 6: damage_control(); break;
			case 7: library_computer(); break;
			case 8: ret = RG_GAME_END_RESIGN; break;
			default:
				puts("ENTER ONE OF THE FOLLOWING:\n"
				     "  NAV  (TO SET COURSE)\n"
				     "  SRS  (FOR SHORT RANGE SENSOR SCAN)\n"
				     "  LRS  (FOR LONG RANGE SENSOR SCAN)\n"
				     "  PHA  (TO FIRE PHASERS)\n"
				     "  TOR  (TO FIRE PHOTON TORPEDOES)\n"
				     "  SHE  (TO RAISE OR LOWER SHIELDS)\n"
				     "  DAM  (FOR DAMAGE CONTROL REPORTS)\n"
				     "  COM  (TO CALL ON LIBRARY-COMPUTER)\n"
				     "  XXX  (TO RESIGN YOUR COMMAND)\n");
				continue;
		}

		if (ret != RG_MAIN_LOOP)
		{
			end_of_game(ret);
			return;
		}
	}
}


// COURSE CONTROL BEGINS HERE
INLINE rg_t course_control()
{
	rg_t ret;
	double C1, // warp/torpedo course
	       W1, // warp factor
	       D6, // damage repair amount when moving
	       T8; // time for warp travel
	vec2   X_12, // course delta;
	       XY;   // enterprise sector while moving

	const char *s_X;

	int N,     // energy cost/distance for navigation
	    I;     // loop variable

	bool D1;   // flag - damage control report started

	printf("COURSE (0-9)? ");
	b_INPUT_1(&C1);

	if (C1 == 9)
		C1 = 1;

	if (C1 < 1 || C1 >= 9) {
		puts("   LT. SULU REPORTS, 'INCORRECT COURSE DATA, SIR!'");
		return RG_MAIN_LOOP;
	}

	s_X = (D[DEVICE_WARP_ENGINES] < 0) ? "0.2" : "8";
	printf("WARP FACTOR (0-%s)? ", s_X);
	b_INPUT_1(&W1);

	if (D[DEVICE_WARP_ENGINES] < 0 && W1 > 0.2) {
		puts("WARP ENGINES ARE DAMAGED.  MAXIUM SPEED = WARP 0.2");
		return RG_MAIN_LOOP;
	}

	if (W1 <= 0 || W1 > 8) {
		if (W1 != 0) {
			printf("   CHIEF ENGINEER SCOTT REPORTS 'THE ENGINES WON'T TAKE"
			       " WARP  %g !'\n", W1);
		}
		return RG_MAIN_LOOP;
	}

	N = W1 * 8 + 0.5;
	if (E - N < 0) {
		printf("ENGINEERING REPORTS   'INSUFFICIENT ENERGY AVAILABLE\n"
		       "                       FOR MANEUVERING AT WARP %g !'\n", W1);

		if (S >= N - E && D[DEVICE_SHIELD_CONTROL] >= 0) {
			printf("DEFLECTOR CONTROL ROOM ACKNOWLEDGES %g UNITS OF ENERGY\n"
			       "                         PRESENTLY DEPLOYED TO SHIELDS.\n", S);
		}

		return RG_MAIN_LOOP;
	}

	// KLINGONS MOVE/FIRE ON MOVING STARSHIP . . .
	for (I = 1; I <= K3; I++) {
		if (K.E[I] == 0)
			continue;

		set_sector(K.XY[I], "   ");
		K.XY[I] = set_empty_sector("+K+");
	}

	if ((ret = klingons_shooting()) != RG_PASS)
		return ret;

	D1 = false;
	D6 = (W1 >= 1) ? 1 : W1;

	for (I = 1; I <= 8; I++) {
		if (D[I] >= 0)
			continue;

		D[I] += D6;

		if (D[I] < 0) {
			if (D[I] > -0.1)
				D[I] = -0.1;

			continue;
		}

		if (!D1) {
			D1 = true;
			printf("DAMAGE CONTROL REPORT:  ");
		}
		printf("%s%s REPAIR COMPLETED.\n", b_SPC(8), get_device_name(I));
	}

	if (b_RND(1) <= 0.2) {
		int R1 = b_FNR(); // device number
		printf("DAMAGE CONTROL REPORT:  %s ", get_device_name(R1));

		if (b_RND(1) < 0.6) {
			D[R1] -= b_RND(1) * 5 + 1;
			puts("DAMAGED\n");
		} else {
			D[R1] += b_RND(1) * 3 + 1;
			puts("STATE OF REPAIR IMPROVED\n");
		}
	}

	// BEGIN MOVING STARSHIP
	set_sector(S_12, "   ");
	X_12 = course_to_delta(C1);
	VEC2_COPY(XY, S_12);

	// the original BASIC code used S1/S2 to track starship position
	// this uses X/Y instead, so S_12 can be int
	for (I = 1; I <= N; I++) {
		int S8;

		VEC2_ADD(XY, XY, X_12);

		if (XY.X < 1 || XY.X >= 9 || XY.Y < 1 || XY.Y >= 9) {
			if ((ret = exceeded_quadrant_limits(N, X_12)) != RG_PASS)
				return ret;
			break;
		}

		S8 = (int)(XY.X) * 24 + (int)(XY.Y) * 3 - 27;

		if (strncmp(s_Q + S8, "  ", 2) != 0) {
			VEC2_SUB(S_12, XY, X_12);
			printf("WARP ENGINES SHUT DOWN AT "
			       "SECTOR %d , %d DUE TO BAD NAVAGATION\n", S_12.X, S_12.Y);
			break;
		}
	}

	// if we didn't exit the quadrant or hit something, store the final
	// coordinates
	if (I > N) {
		VEC2_COPY(S_12, XY);
	}

	set_sector(S_12, "<*>");
	maneuver_energy(N);

	T8 = (W1 < 1) ? 0.1 * (int)(10 * W1) : 1;

	T += T8;
	if (T > T0 + T9)
		return RG_GAME_END_TIME;

	// SEE IF DOCKED, THEN GET COMMAND
	short_range_sensors_dock();
	return RG_MAIN_LOOP;
}

// EXCEEDED QUADRANT LIMITS
INLINE rg_t exceeded_quadrant_limits(int N, vec2 X_12)
{
	ivec2 Q_45,  // enterprise X, Y quadrant before moving
	        XY;  // enterprise X, Y absolute sector
	bool X5;     // flag - left galaxy

	Q_45 = Q_12;
	VEC2_MAD(XY, S_12, 8, Q_12);
	VEC2_MAD(XY, XY, N, X_12);
	Q_12.X = XY.X / 8; Q_12.Y = XY.Y / 8;
	S_12.X = XY.X % 8; S_12.Y = XY.Y % 8;

	if (S_12.X == 0) {
		Q_12.X--;
		S_12.X = 8;
	}

	if (S_12.Y == 0) {
		Q_12.Y--;
		S_12.Y = 8;
	}

	X5 = false;

	if (Q_12.X < 1) {
		X5 = true;
		Q_12.X = 1;
		S_12.X = 1;
	}

	if (Q_12.X > 8) {
		X5 = true;
		Q_12.X = 8;
		S_12.X = 8;
	}

	if (Q_12.Y < 1) {
		X5 = true;
		Q_12.Y = 1;
		S_12.Y = 1;
	}

	if (Q_12.Y > 8) {
		X5 = true;
		Q_12.Y = 8;
		S_12.Y = 8;
	}

	if (X5) {
		printf("LT. UHURA REPORTS MESSAGE FROM STARFLEET COMMAND:\n"
		       "  'PERMISSION TO ATTEMPT CROSSING OF GALACTIC PERIMETER\n"
		       "  IS HEREBY *DENIED*.  SHUT DOWN YOUR ENGINES.'\n"
		       "CHIEF ENGINEER SCOTT REPORTS  'WARP ENGINES SHUT DOWN\n"
		       "  AT SECTOR %d , %d OF QUADRANT %d , %d .'\n",
		       S_12.X, S_12.Y, Q_12.X, Q_12.Y);
		if (T > T0 + T9)
			return RG_GAME_END_TIME;
	}

	if (8 * Q_12.X + Q_12.Y == 8 * Q_45.X + Q_45.Y)
		return RG_PASS;

	T++;
	maneuver_energy(N);
	new_quadrant();
	short_range_sensors_dock();
	return RG_MAIN_LOOP;
}


// MANEUVER ENERGY S/R **
void maneuver_energy(int N)
{
	E -= N + 10;

	if (E >= 0)
		return;

	puts("SHIELD CONTROL SUPPLIES ENERGY TO COMPLETE THE MANEUVER.");
	S += E;
	E = 0;

	if (S <= 0)
		S = 0;
}


// LONG RANGE SENSOR SCAN CODE
INLINE void long_range_sensors()
{
	int I, // loop variable
	    J; // loop variable 2

	const char *s_O1; // horizontal rule

	if (D[DEVICE_LONG_RANGE_SENSORS] < 0) {
		puts("LONG RANGE SENSORS ARE INOPERABLE");
		return;
	}

	printf("LONG RANGE SCAN FOR QUADRANT %d , %d\n", Q_12.X, Q_12.Y);
	s_O1 = "-------------------";
	puts(s_O1);
	for (I = Q_12.X - 1; I <= Q_12.X + 1; I++) {
		for (J = Q_12.Y - 1; J <= Q_12.Y + 1; J++) {
			printf(": ");
			if (I > 0 && I < 9 && J > 0 && J < 9) {
				printf("%03d ", G[I][J]);
				Z[I][J] = G[I][J];
			} else {
				printf("*** ");
			}
		}
		printf(":\n%s\n", s_O1);
	}
}

void no_enemy_ships()
{
	printf("SCIENCE OFFICER SPOCK REPORTS  'SENSORS SHOW NO ENEMY SHIPS\n"
	       "                                IN THIS QUADRANT'\n");
}


// PHASER CONTROL CODE BEGINS HERE
INLINE rg_t phaser_control()
{
	rg_t ret;
	int I,    // loop variable
	    H1;   // phaser power divided by # of klingons in quadrant
	double X; // energy to fire

	if (D[DEVICE_PHASER_CONTROL] < 0) {
		puts("PHASERS INOPERATIVE");
		return RG_MAIN_LOOP;
	}

	if (K3 <= 0) {
		no_enemy_ships();
		return RG_MAIN_LOOP;
	}

	if (D[DEVICE_LIBRARY_COMPUTER] < 0)
		puts("COMPUTER FAILURE HAMPERS ACCURACY");

	printf("PHASERS LOCKED ON TARGET;  ");

	do {
		printf("ENERGY AVAILABLE = %g UNITS\n"
		       "NUMBER OF UNITS TO FIRE? ", E);
		b_INPUT_1(&X);

		if (X <= 0)
			return RG_MAIN_LOOP;
	} while (E - X < 0);

	E -= X;

	// BUG: should check D[DEVICE_LIBRARY_COMPUTER]
	if (D[DEVICE_SHIELD_CONTROL] < 0)
		X *= b_RND(1);

	H1 = X / K3;
	for (I = 1; I <= 3; I++) {
		int H; // phaser damage to klingon

		if (K.E[I] <= 0)
			continue;

		H = H1 / b_FND(I) * (b_RND(1) + 2);

		if (H <= 0.15 * K.E[I]) {
			printf("SENSORS SHOW NO DAMAGE TO ENEMY AT  %d , %d\n", K.XY[I].X, K.XY[I].Y);
			continue;
		}

		K.E[I] -= H;
		printf(" %d UNIT HIT ON KLINGON AT SECTOR %d ,"
		       " %d\n", H, K.XY[I].X, K.XY[I].Y);

		if (K.E[I] > 0) {
			printf("   (SENSORS SHOW %g UNITS REMAINING)\n", K.E[I]);
			continue;
		}

		puts("*** KLINGON DESTROYED ***");
		K3--; K9--;
		set_sector(K.XY[I], "   ");
		K.E[I] = 0;
		G[Q_12.X][Q_12.Y] -= 100;
		Z[Q_12.X][Q_12.Y] = G[Q_12.X][Q_12.Y];

		if (K9 <= 0)
			return RG_GAME_END_NO_KLINGONS;
	}

	if ((ret = klingons_shooting()) != RG_PASS)
		return ret;
	return RG_MAIN_LOOP;
}


// PHOTON TORPEDO CODE BEGINS HERE
INLINE rg_t photon_torpedo()
{
	rg_t ret;
	int I;     // loop variable
	double C1; // torpedo course
	ivec2 XY3; // torpedo sector (int)
	vec2 XY,   // torpedo sector (non-int)
	     X_12; // course delta

	if (P <= 0) {
		puts("ALL PHOTON TORPEDOES EXPENDED");
		return RG_MAIN_LOOP;
	}

	if (D[DEVICE_PHOTON_TUBES] < 0) {
		puts("PHOTON TUBES ARE NOT OPERATIONAL");
		return RG_MAIN_LOOP;
	}

	while(1) {
		printf("PHOTON TORPEDO COURSE (1-9)? ");
		b_INPUT_1(&C1);

		if (C1 == 9)
			C1 = 1;

		if (C1 < 1 || C1 >= 9) {
			puts("ENSIGN CHEKOV REPORTS,  'INCORRECT COURSE DATA, SIR!'");
			return RG_MAIN_LOOP;
		}

		X_12 = course_to_delta(C1);
		E -= 2;
		P--;
		VEC2_COPY(XY, S_12);

		puts("TORPEDO TRACK:");
		do {
			VEC2_ADD(XY, XY, X_12);
			XY3.X = XY.X + 0.5; XY3.Y = XY.Y + 0.5;

			if (XY3.X < 1 || XY3.X > 8 || XY3.Y < 1 || XY3.Y > 8) {
				puts("TORPEDO MISSED");
				if ((ret = klingons_shooting()) != RG_PASS)
					return ret;
				return RG_MAIN_LOOP;
			}

			printf("                %d , %d\n", XY3.X, XY3.Y);
		} while (is_sector(XY3, "   "));

		if (is_sector(XY3, "+K+")) {
			puts("*** KLINGON DESTROYED ***");
			K3--; K9--;

			if (K9 <= 0)
				return RG_GAME_END_NO_KLINGONS;

			for (I = 1; I <= 3; I++) {
				if (XY3.X == K.XY[I].X && XY3.Y == K.XY[I].Y)
					break;
			}

			if (I > 3)
				I = 3;

			K.E[I] = 0;
		} else if (is_sector(XY3, " * ")) {
			printf("STAR AT %d , %d ABSORBED TORPEDO ENERGY.\n", XY3.X, XY3.Y);

			if ((ret = klingons_shooting()) != RG_PASS)
				return ret;
			return RG_MAIN_LOOP;
		} else if (is_sector(XY3, ">!<")) {
			puts("*** STARBASE DESTROYED ***");
			B3--; B9--;

			if (B9 <= 0 && K9 <= T - T0 - T9)
				return RG_GAME_END_TORPEDOED_STARBASE;

			puts("STARFLEET COMMAND REVIEWING YOUR RECORD TO CONSIDER\n"
			     "COURT MARTIAL!");
			D0 = false;
		} else {
			// if you blew up something that isn't a klingon, star, or starbase, you get to fire again?
			continue;
		}
		break;
	}

	set_sector(XY3, "   ");
	G[Q_12.X][Q_12.Y] = K3 * 100 + B3 * 10 + S3;
	Z[Q_12.X][Q_12.Y] = G[Q_12.X][Q_12.Y];

	if ((ret = klingons_shooting()) != RG_PASS)
		return ret;
	return RG_MAIN_LOOP;
}


// SHIELD CONTROL
INLINE void shield_control()
{
	double X; // desired shield strength

	if (D[DEVICE_SHIELD_CONTROL] < 0) {
		puts("SHIELD CONTROL INOPERABLE");
		return;
	}

	printf("ENERGY AVAILABLE = %g "
	        "NUMBER OF UNITS TO SHIELDS? ", E + S);
	b_INPUT_1(&X);

	if (X < 0 || S == X) {
		puts("<SHIELDS UNCHANGED>");
		return;
	}

	if (X > E + S) {
		puts("SHIELD CONTROL REPORTS  'THIS IS NOT THE FEDERATION TREASURY.'\n"
		     "<SHIELDS UNCHANGED>");
		return;
	}

	E += S - X;
	S = X;

	printf("DEFLECTOR CONTROL ROOM REPORT:\n"
	       "  'SHIELDS NOW AT %d UNITS PER YOUR COMMAND.'\n", (int)(S));
}


rg_t starbase_repair()
{
	double D3; // estimated starbase repair time
	int I;
	char s_A[2];

	D3 = 0;
	for (I = 1; I <= 8; I++) {
		if (D[I] < 0)
			D3 += 0.1;
	}

	if (D3 == 0)
		return RG_MAIN_LOOP;

	linefeeds(1);
	D3 += D4;

	if (D3 >= 1)
		D3 = 0.9;

	printf("TECHNICIANS STANDING BY TO EFFECT REPAIRS TO YOUR SHIP;\n"
	       "ESTIMATED TIME TO REPAIR: %g STARDATES\n"
	       "WILL YOU AUTHORIZE THE REPAIR ORDER (Y/N)? ", 0.01 * (int)(100 * D3));
	b_INPUT_S(s_A, 2);

	if (my_strnicmp(s_A, "Y", 1) != 0)
		return RG_MAIN_LOOP;

	for (I = 1; I <= 8; I++) {
		if (D[I] < 0)
			D[I] = 0;
	}

	T += D3 + 0.1;
	return RG_PASS;
}

void damage_report()
{
	int R1; // device number

	puts("\n"
	     "DEVICE             STATE OF REPAIR");

	for (R1 = 1; R1 <= 8; R1++) {
		const char *s_G2 = get_device_name(R1);
		printf("%s%s%g\n", s_G2, b_SPC(26 - strlen(s_G2)), (int)(D[R1] * 100) * 0.01);
	}

	linefeeds(1);
}

// DAMAGE CONTROL
INLINE void damage_control()
{
	rg_t ret;

	if (D[DEVICE_DAMAGE_CONTROL] < 0) {
		puts("DAMAGE CONTROL REPORT NOT AVAILABLE");

		if (!D0)
			return;

		if ((ret = starbase_repair()) == RG_MAIN_LOOP)
			return;
	}

	damage_report();

	if (D0)
	{
		if ((ret = starbase_repair()) == RG_MAIN_LOOP)
			return;

		damage_report();
	}
}


// KLINGONS SHOOTING
rg_t klingons_shooting()
{
	int I;

	if (K3 <= 0)
		return RG_PASS;

	if (D0) {
		puts("STARBASE SHIELDS PROTECT THE ENTERPRISE");
		return RG_PASS;
	}

	for (I = 1; I <= 3; I++) {
		int H,  // phaser damage from klingon
		    R1; // device number

		if (K.E[I] <= 0)
			continue;

		H = (K.E[I] / b_FND(I)) * (2 + b_RND(1));
		S -= H;
		K.E[I] /= 3 + b_RND(0);
		printf(" %d UNIT HIT ON ENTERPRISE FROM SECTOR %d , %d\n", H, K.XY[I].X, K.XY[I].Y);

		if (S <= 0)
			return RG_GAME_END_ENTERPRISE_DESTROYED;

		printf("      <SHIELDS DOWN TO %g UNITS>\n", S);

		if (H < 20)
			continue;

		if (b_RND(1) > 0.6 || H / S <= 0.02)
			continue;

		R1 = b_FNR();
		D[R1] -= H / S + 0.5 * b_RND(1);
		printf("DAMAGE CONTROL REPORTS %s DAMAGED BY THE HIT'\n", get_device_name(R1));
	}

	return RG_PASS;
}


// END OF GAME
INLINE void end_of_game(rg_t end_type)
{
	switch (end_type)
	{
		case RG_GAME_END_NO_ENERGY:
			puts("\n"
			     "** FATAL ERROR **   YOU'VE JUST STRANDED YOUR SHIP IN\n"
			     "SPACE\n"
			     "YOU HAVE INSUFFICIENT MANEUVERING ENERGY,"
			     " AND SHIELD CONTROL\n"
			     "IS PRESENTLY INCAPABLE OF CROSS"
			     "-CIRCUITING TO ENGINE ROOM!!");
			break;

		case RG_GAME_END_TORPEDOED_STARBASE:
			puts("THAT DOES IT, CAPTAIN!!  YOU ARE HEREBY RELIEVED OF COMMAND\n"
			     "AND SENTENCED TO 99 STARDATES AT HARD LABOR ON CYGNUS 12!!");

		default:
			break;
	}

	switch(end_type)
	{
		case RG_GAME_END_NO_KLINGONS:
			printf("CONGRULATION, CAPTAIN!  THEN LAST KLINGON BATTLE CRUISER\n"
			        "MENACING THE FDERATION HAS BEEN DESTROYED.\n\n"
			        "YOUR EFFICIENCY RATING IS %g\n", 1000 * (K7 / (T - T0)) * (K7 / (T - T0)));
			break;

		case RG_GAME_END_ENTERPRISE_DESTROYED:
			puts("\n"
			     "THE ENTERPRISE HAS BEEN DESTROYED.  THEN FEDERATION "
			     "WILL BE CONQUERED");

		case RG_GAME_END_TIME:
		case RG_GAME_END_NO_ENERGY:
			printf("IT IS STARDATE %g\n", T);

		case RG_GAME_END_RESIGN:
		case RG_GAME_END_TORPEDOED_STARBASE:
			printf("THERE WERE %d KLINGON BATTLE CRUISERS LEFT AT\n"
			        "THE END OF YOUR MISSION.\n", K9);

		default:
			break;
	}

	linefeeds(2);

	if (B9 > 0) {
		char s_A[4];
		printf("THE FEDERATION IS IN NEED OF A NEW STARSHIP COMMANDER\n"
		        "FOR A SIMILAR MISSION -- IF THERE IS A VOLUNTEER,\n"
		        "LET HIM STEP FORWARD AND ENTER 'AYE'? ");
		b_INPUT_S(s_A, 4);

		if (my_strnicmp(s_A, "AYE", 3) == 0)
			return;
	}

	exit(0);
}

INLINE bool next_to_starbase()
{
	ivec2 IJ;

	for (IJ.X = S_12.X - 1; IJ.X <= S_12.X + 1; IJ.X++) {
		for (IJ.Y = S_12.Y - 1; IJ.Y <= S_12.Y + 1; IJ.Y++) {
			if (IJ.X < 1 || IJ.X > 8 || IJ.Y < 1 || IJ.Y > 8)
				continue;

			if (is_sector(IJ, ">!<"))
				return true;
		}
	}
	return false;
}

// SHORT RANGE SENSOR SCAN & STARTUP SUBROUTINE
void short_range_sensors_dock()
{
	const char *s_C,  // alert state (GREEN, *RED*, YELLOW, DOCKED)
	           *s_O1, // horizontal rule
		   *p;
	int I, J;

	D0 = false;
	if (next_to_starbase()) {
		D0 = true;
		s_C = "DOCKED";
		E = E0;
		P = P0;
		puts("SHIELDS DROPPED FOR DOCKING PURPOSES");
		S = 0;
	} else if (K3 > 0) {
		s_C = "*RED*";
	} else {
		s_C = "GREEN";

		if (E < E0 * 0.1)
			s_C = "YELLOW";
	}

	if (D[DEVICE_SHORT_RANGE_SENSORS] < 0) {
		puts("\n*** SHORT RANGE SENSORS ARE OUT ***\n");
		return;
	}

	s_O1 = "---------------------------------";
	puts(s_O1);

	p = s_Q;
	for (I = 0; I < 8; I++) {
		const char *info[8] = { "STARDATE", "CONDITION", "QUADRANT",
			"SECTOR", "PHOTON TORPEDOES", "TOTAL ENERGY", "SHIELDS",
			"KLINGONS REMAINING"
		};

		for (J = 0; J < 8; J++) {
			putchar(' ');
			putchar(*p++);
			putchar(*p++);
			putchar(*p++);
		}

		printf("%s%s%s", b_SPC(8), info[I], b_SPC(19 - strlen(info[I])));
		switch(I)
		{
			case 0:  printf("%g\n", (int)(T * 10) * 0.1); break;
			case 1:  puts(s_C); break;
			case 2:  printf("%d , %d\n", Q_12.X, Q_12.Y); break;
			case 3:  printf("%d , %d\n", S_12.X, S_12.Y); break;
			case 4:  printf("%d\n", P); break;
			case 5:  printf("%d\n", (int)(E + S)); break;
			case 6:  printf("%d\n", (int)(S)); break;
			default: printf("%d\n", K9);
		}
	}
	puts(s_O1);
}


// LIBRARY COMPUTER CODE
INLINE void library_computer()
{
	if (D[DEVICE_LIBRARY_COMPUTER] < 0) {
		puts("COMPUTER DISABLED");
		return;
	}

	while(1)
	{
		double A; // library-computer function input

		printf("COMPUTER ACTIVE AND AWAITING COMMAND? ");
		b_INPUT_1(&A);

		if (A < 0)
			return;
		linefeeds(1);

		switch((int)(A)) {
			case 0: galaxy_map(GALAXY_MAP_RECORD); break;
			case 1: status_report(); break;
			case 2: dir_calc_klingons(); break;
			case 3: dir_calc_starbase(); break;
			case 4: dir_calc_input(); break;
			case 5: galaxy_map(GALAXY_MAP_NAMES); break;
			default:
				puts("FUNCTIONS AVAILABLE FROM LIBRARY-COMPUTER:\n"
				     "   0 = CUMULATIVE GALACTIC RECORD\n"
				     "   1 = STATUS REPORT\n"
				     "   2 = PHOTON TORPEDO DATA\n"
				     "   3 = STARBASE NAV DATA\n"
				     "   4 = DIRECTION/DISTANCE CALCULATOR\n"
				     "   5 = GALAXY 'REGION NAME' MAP\n");
				continue;
		}
		break;
	}
}


// GALAXY MAP/CUMULATIVE GALACTIC RECORD
INLINE void galaxy_map(gm_t map_type)
{
	int I, J;
	const char *s_O1; // horizontal rule

	if (map_type != GALAXY_MAP_RECORD)
	{
		// SETUP TO CHANGE CUM GAL RECORD TO GALAXY MAP
		printf("%sTHE GALAXY\n", b_SPC(24));
	} else {
		// CUM GALACTIC RECORD
		// INPUT"DO YOU WANT A HARDCOPY? IS THE TTY ON (Y/N)";A$
		// IFA$="Y"THENPOKE1229,2:POKE1237,3:NULL1
		printf("\n"
		        "        "
		        "COMPUTER RECORD OF GALAXY FOR QUADRANT %d , %d\n"
		        "\n", Q_12.X, Q_12.Y);
	}

	puts("       1     2     3     4     5     6     7     8");
	s_O1 = "     ----- ----- ----- ----- ----- ----- ----- -----";
	puts(s_O1);
	for (I = 1; I <= 8; I++) {
		printf(" %d ", I);
		if (map_type == GALAXY_MAP_RECORD) {
			for (J = 1; J <= 8; J++) {
				printf("   ");

				if (Z[I][J] == 0) {
					printf("***");
					continue;
				}

				printf("%03d", Z[I][J]);
			}
		} else {
			int J0, J1; // spacing of names on galaxy map
			const char *s_G2_0, *s_G2_1; // quadrant names

			s_G2_0 = get_quadrant_name(I, 1);
			s_G2_1 = get_quadrant_name(I, 5);
			J0 = 12 - (strlen(s_G2_0) + 1) / 2;
			J1 = 36 - J0 - strlen(s_G2_0) - (strlen(s_G2_1) + 1) / 2;

			printf("%s%s%s%s", b_SPC(J0), s_G2_0, b_SPC(J1), s_G2_1);
		}
		printf("\n%s\n", s_O1);
	}
	linefeeds(1);
}


// STATUS REPORT
INLINE void status_report()
{
	const char *s_X; // plural

	puts("   STATUS REPORT:");

	s_X = (K9 > 1) ? "S" : "";
	printf("KLINGON%s LEFT:  %d\n"
	       "MISSION MUST BE COMPLETED IN %g STARDATES\n",
		s_X, K9, 0.1 * (int)((T0 + T9 - T) * 10));

	if (B9 < 1) {
		puts("YOUR STUPIDITY HAS LEFT YOU ON YOUR ON IN\n"
		     "  THE GALAXY -- YOU HAVE NO STARBASES LEFT!");
		damage_control();
		return;
	}

	s_X = (B9 > 1) ? "S" : "";
	printf("THE FEDERATION IS MAINTAINING %d STARBASE%s IN THE GALAXY\n", B9, s_X);
	damage_control();
}


void actual_dir_calc(double C1, double A, double W1, double X)
{
	X = X - A; A = W1 - C1;

	C1 = (X > 0) ? ((A > 0) ? 8 : 2) : ((A > 0) ? 6 : 4);
	W1 = fabs(X) - fabs(A);
	W1 /= fabs(X) > fabs(A) ? fabs(X) : fabs(A);
	C1 += ((X > 0) == (A > 0)) ? W1 : -W1;

	printf("DIRECTION = %g\n"
	       "DISTANCE = %g\n", C1, sqrt(X * X + A * A));
}


// TORPEDO, BASE NAV, D/D CALCULATOR
INLINE void dir_calc_klingons()
{
	int I;
	const char *s_X; // plural

	if (K3 <= 0) {
		no_enemy_ships();
		return;
	}

	s_X = (K3 > 1) ? "S" : "";
	printf("FROM ENTERPRISE TO KLINGON BATTLE CRUSER%s\n", s_X);

	for (I = 1; I <= 3; I++) {
		if (K.E[I] <= 0)
			continue;

		actual_dir_calc(S_12.X, S_12.Y, K.XY[I].X, K.XY[I].Y);
	}
}

INLINE void dir_calc_starbase()
{
	if (B3 == 0) {
		puts("MR. SPOCK REPORTS,  'SENSORS SHOW NO STARBASES IN THIS"
		     " QUADRANT.'");
		return;
	}

	puts("FROM ENTERPRISE TO STARBASE:");
	actual_dir_calc(S_12.X, S_12.Y, B_45.X, B_45.Y);
}

INLINE void dir_calc_input()
{
	double C1, // initial X coord
		A,  // initial Y coord
		W1, // final X coord
		X;  // final Y coord

	printf("DIRECTION/DISTANCE CALCULATOR:\n"
	       "YOU ARE AT QUADRANT  %d , %d  SECTOR  %d , %d\n"
	       "PLEASE ENTER\n"
	       "  INITIAL COORDINATES (X,Y)? ", Q_12.X, Q_12.Y, S_12.X, S_12.Y);
	b_INPUT_2(&C1, &A);
	printf("  FINAL COORDINATES (X,Y)? ");
	b_INPUT_2(&W1, &X);
	actual_dir_calc(C1, A, W1, X);
}


// FIND EMPTY PLACE IN QUADRANT (FOR THINGS)
INLINE ivec2 set_empty_sector(const char *s_A)
{
	ivec2 R_12;

	do {
		R_12.X = b_FNR(); R_12.Y = b_FNR();
	} while (!is_sector(R_12, "   "));

	set_sector(R_12, s_A);

	return R_12;
}


// INSERT IN STRING ARRAY FOR QUADRANT
INLINE void set_sector(ivec2 Z_12, const char *s_A)
{
	int S8 = (Z_12.Y + Z_12.X * 8) * 3 - 27;

	assert(strlen(s_A) == 3);

	s_Q[S8] = s_A[0];
	s_Q[S8 + 1] = s_A[1];
	s_Q[S8 + 2] = s_A[2];
}


// PRINTS DEVICE NAME
const char *get_device_name(int R1)
{
	const char *device_names[] = {
		"WARP ENGINES", "SHORT RANGE SENSORS", "LONG RANGE SENSORS",
		"PHASER CONTROL", "PHOTON TUBES", "DAMAGE CONTROL",
		"SHIELD CONTROL", "LIBRARY-COMPUTER"
	};

	if (R1 >= 1 && R1 <= 8)
		return device_names[R1 - 1];
	return device_names[0];
}


// STRING COMPARISON IN QUADRANT ARRAY
INLINE bool is_sector(ivec2 Z_12, const char *s_A)
{
	int S8 = (Z_12.Y + Z_12.X * 8) * 3 - 27;

	return (strncmp(s_Q + S8, s_A, 3) == 0);
}

// QUADRANT NAME IN G2$ FROM Z4,Z5 (=Q1,Q2)
const char *get_quadrant_name(int Z4, int Z5)
{
	const char *quadrant_names[] = {
		"ANTARES", "RIGEL", "PROCYON", "VEGA", "CANOPUS", "ALTAIR",
		"SAGITTARIUS", "POLLUX", "SIRIUS", "DENEB", "CAPELLA",
		"BETELGEUSE", "ALDEBARAN", "REGULUS", "ARCTURUS", "SPICA"
	};

	assert(Z4 >= 1 && Z4 <= 8);

	return quadrant_names[Z4 + ((Z5 <= 4) ? -1 : 7)];
}

INLINE const char *get_quadrant_number(int Z5)
{
	const char *quadrant_numbers[] = {
		"I", "II", "III", "IV"
	};

	assert(Z5 >= 1 && Z5 <= 8);

	return quadrant_numbers[(Z5 - 1) % 4];
}

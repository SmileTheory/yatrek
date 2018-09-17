#ifdef __cplusplus
#include <cstdio>
#include <cmath>
#include <ctime>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#else
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#endif

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

void raninit(ranctx *x, u4 seed)
{
	u4 i;

	x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
	for (i = 0; i < 20; ++i)
		(void)ranval(x);
}

////////////////////////////////////////////////////////////////////////////////

ranctx rstate;

float b_RND(double R)
{
	return (ranval(&rstate) >> 8) * (1. / (1 << 24));
}

const char *b_SPC(int len)
{
	const char *space = "                                                                                ";

	if (len > 80 || len < 0)
		return NULL;

	return space + 80 - len;
}

void b_INPUT_1(double *num)
{
	char s[80];
	fflush(stdout);
	fgets(s, sizeof(s), stdin);
	s[79] = '\0';
	sscanf(s, "%lf", num);
}

void b_INPUT_2(double *num1, double *num2)
{
	char s[80];
	fflush(stdout);
	fgets(s, sizeof(s), stdin);
	s[79] = '\0';
	sscanf(s, "%lf,%lf", num1, num2);
}

void b_INPUT_S(char *out, int size)
{
	char s[80];
	int i;

	fflush(stdout);
	fgets(s, 80, stdin);
	s[79] = '\0';

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

/*
 * int variables notes:
 *
 * K[?][3] can be non-int, although [?][1] and [?][2] cannot.
 *
 * E can be non-int, because there is no check for firing phasers at non-int
 * strength, and there is no check for setting shield strength to non-int.
 *
 * (If energy transfer from shields to warp drive is enabled)
 * S can go non-int because E can be non-int and insufficient to perform a
 * maneuver, and then added to S.
 *
 * S1, S2 are mostly int, but can go non-int temporarily during navigation.
 *
 * S9, E0 are technically always int, but are basically constants so have
 * been left as double for flexibility.
 *
 * Z1, Z2 are usually int, but can go non-int during torpedo tracking.
 *
 * R2 is always int, but R1 can be non-int and they are commonly used
 * together.
 */

// course calc constants
const int C[10][3] = {
	{ 0, 0, 0}, { 0, 0, 1}, { 0, -1, 1}, { 0, -1, 0}, { 0, -1, -1},
	{ 0, 0, -1}, { 0, 1, -1}, { 0, 1, 0}, { 0, 1, 1}, { 0, 0, 1}
};

int    G[9][9],  // galaxy info: 100 * klingons + 10 * starbases + stars
       Z[9][9];  // known galaxy info, same format as G[][]

double K[4][4],  // klingon info: [1] = x [2] = y [3] = power
       D[9];     // devices state of repair, < 0 -> damaged

double T,  // current stardate
       T0, // initial stardate
       T9, // remaining stardates

       E,  // current energy
       E0, // max energy
       S,  // shield strength
       S1, // enterprise X sector
       S2, // enterprise Y sector

       S9, // average klingon energy

       R1, // random sector(): X result
           // galaxy gen: random number
           // damage control: device number
           // klingons shooting: device number
           // repair: device number
       R2, // random sector(): Y result

       D4; // random extra starbase repair time

int    Q1, // enterprise X quadrant
       Q2, // enterprise Y quadrant

       P,  // current # of torpedoes
       P0, // max # of torpedoes

       K3, // # of klingons in quadrant
       K7, // starting # of klingons in galaxy
       K9, // current # of klingons in galaxy

       B3, // # of starbases in quadrant
       B4, // starbase X sector
       B5, // starbase Y sector
       B9, // current # of starbases in galaxy

       S3, // # of stars in quadrant

       S8; // position in quadrant string array

bool D0; // flag - docked

// list of commands
const char *s_A1 = "NAVSRSLRSPHATORSHEDAMCOMXXX";

char s_Q[192];  // quadrant string

double b_FND(int I)
{
	return sqrt((K[I][1] - S1) * (K[I][1] - S1) + (K[I][2] - S2) * (K[I][2] - S2));
}

double b_FNR(double R)
{
	return (int)(b_RND(R) * 7.98 + 1.01);
}

typedef enum {
	GALAXY_MAP_NAMES,
	GALAXY_MAP_RECORD,
} gm_t;

typedef enum {
	DIR_CALC_KLINGONS,
	DIR_CALC_STARBASE,
	DIR_CALC_INPUT
} dc_t;

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

void new_galaxy();
void new_quadrant();
void main_loop();
rg_t course_control();
rg_t exceeded_quadrant_limits(int N, double X, double Y, double X1, double X2);
void maneuver_energy(int N);
void long_range_sensors();
rg_t phaser_control();
rg_t photon_torpedo();
void shield_control();
void damage_control();
rg_t klingons_shooting();
void end_of_game(rg_t end_type);
void short_range_sensors_dock();
void library_computer();
void galaxy_map(gm_t map_type);
void status_report();
void dir_calc(dc_t calc_type);
void get_empty_sector();
void set_sector(double Z1, double Z2, const char *s_A);
const char *get_device_name(int R1);
bool is_sector(double Z1, double Z2, const char *s_A);
const char *get_quadrant_name(int Z4, int Z5);
const char *get_quadrant_number(int Z5);

void linefeeds(int num)
{
	while (num--)
		printf("\n");
}

void instructions()
{
	char s_K[2];

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

	if (stricmp(s_K, "N") == 0)
		return;

	printf( "\n"
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
	        "\n");
	linefeeds(23 - 20);
	printf("-PRESS ENTER TO CONTINUE-");
	b_INPUT_S(s_K, 2);

	printf( "     YOU HAVE THE FOLLOWING COMMANDS AVAILABLE TO YOU AS CAPTAIN\n"
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
	        "     ONE WARP FACTOR IS THE SIZE OF \n"
	        "     ONE QUADTANT.  THEREFORE, TO GET\n"
	        "     FROM QUADRANT 6,5 TO 5,5, YOU WOULD\n"
	        "     USE COURSE 3, WARP FACTOR 1.\n"
	        "\n");
	linefeeds(23 - 18);
	printf("-PRESS ENTER TO CONTINUE-");
	b_INPUT_S(s_K, 2);

	printf( "\\SRS\\ COMMAND = SHORT RANGE SENSOR SCAN\n"
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
	        "\n");
	linefeeds(23 - 21);
	printf("-PRESS ENTER TO CONTINUE-");
	b_INPUT_S(s_K, 2);

	printf( "\\PHA\\ COMMAND = PHASER CONTROL.\n"
	        "     ALLOWS YOU TO DESTROY THE KLINGON BATTLE CRUISERS BY \n"
	        "     ZAPPING THEM WITH SUITABLY LARGE UNITS OF ENERGY TO\n"
	        "     DEPLETE THEIR SHIELD POWER.  (REMBER, KLINGONS HAVE\n"
	        "     PHASERS TOO!)\n"
	        "\n"
	        "\\TOR\\ COMMAND = PHOTON TORPEDO CONTROL\n"
	        "     TORPEDO COURSE IS THE SAME AS USED IN WARP ENGINE CONTROL\n"
	        "     IF YOU HIT THE KLINGON VESSEL, HE IS DESTROYED AND\n"
	        "     CANNOT FIRE BACK AT YOU.  IF YOU MISS, YOU ARE SUBJECT TO\n"
	        "     HIS PHASER FIRE.  IN EITHER CASE, YOU ARE ALSO SUBJECT TO \n"
	        "     THE PHASER FIRE OF ALL OTHER KLINGONS IN THE QUADRANT.\n"
	        "\n"
	        "     THE LIBRARY-COMPUTER (\\COM\\ COMMAND) HAS AN OPTION TO \n"
	        "     COMPUTE TORPEDO TRAJECTORY FOR YOU (OPTION 2)\n"
	        "\n");
	linefeeds(23 - 16);
	printf("-PRESS ENTER TO CONTINUE-");
	b_INPUT_S(s_K, 2);

	printf( "\\SHE\\ COMMAND = SHIELD CONTROL\n"
	        "     DEFINES THE NUMBER OF ENERGY UNITS TO BE ASSIGNED TO THE\n"
	        "     SHIELDS.  ENERGY IS TAKEN FROM TOTAL SHIP'S ENERGY.  NOTE\n"
	        "     THAN THE STATUS DISPLAY TOTAL ENERGY INCLUDES SHIELD ENERGY\n"
	        "\n"
	        "\\DAM\\ COMMAND = DAMMAGE CONTROL REPORT\n"
	        "     GIVES THE STATE OF REPAIR OF ALL DEVICES.  WHERE A NEGATIVE\n"
	        "     'STATE OF REPAIR' SHOWS THAT THE DEVICE IS TEMPORARILY\n"
	        "     DAMAGED.\n"
	        "\n");
	linefeeds(23 - 10);
	printf("-PRESS ENTER TO CONTINUE-");
	b_INPUT_S(s_K, 2);

	printf( "\\COM\\ COMMAND = LIBRARY-COMPUTER\n"
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
	        "        THIS OPTION GIVES DIRECTION AND DISTANCE TO ANY \n"
	        "        STARBASE WITHIN YOUR QUADRANT\n"
	        "     OPTION 4 = DIRECTION/DISTANCE CALCULATOR\n"
	        "        THIS OPTION ALLOWS YOU TO ENTER COORDINATES FOR\n"
	        "        DIRECTION/DISTANCE CALCULATIONS\n"
	        "     OPTION 5 = GALACTIC /REGION NAME/ MAP\n"
	        "        THIS OPTION PRINTS THE NAMES OF THE SIXTEEN MAJOR \n"
	        "        GALACTIC REGIONS REFERRED TO IN THE GAME.\n");
	linefeeds(23 - 20);
	printf("-PRESS ENTER TO CONTINUE-");
	b_INPUT_S(s_K, 2);
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
		printf( "\n\n\n\n\n\n\n\n\n\n\n"
		        "                                    ,------*------,\n"
		        "                    ,-------------   '---  ------'\n"
		        "                     '-------- --'      / /\n"
		        "                         ,---' '-------/ /--,\n"
		        "                          '----------------'\n\n"
		        "                    THE USS ENTERPRISE --- NCC-1701\n"
		        "\n\n\n\n\n");

		new_galaxy();
		new_quadrant();
		short_range_sensors_dock();
		main_loop();
	}
}


void new_galaxy()
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
	s_X0 = " IS ";

	// INITIALIZE ENTERPRIZE'S POSITION
	Q1 = b_FNR(1); Q2 = b_FNR(1);
	S1 = b_FNR(1); S2 = b_FNR(1);

	for (I = 1; I <= 8; I++)
		D[I] = 0;

	// SETUP WHAT EXHISTS IN GALAXY . . .
	// K3= # KLINGONS  B3= # STARBASES  S3 = # STARS
	for (I = 1; I <= 8; I++) {
		for (J = 1; J <= 8; J++) {
			Z[I][J] = 0;

			R1 = b_RND(1);
			K3 = (R1 > 0.98) ? 3 : (R1 > 0.95) ? 2 : (R1 > 0.8) ? 1 : 0;
			K9 += K3;

			B3 = (b_RND(1) > 0.96) ? 1 : 0;
			B9 += B3;

			G[I][J] = K3 * 100 + B3 * 10 + b_FNR(1);
		}
	}

	if (K9 > T9)
		T9 = K9 + 1;

	if (B9 == 0) {
		if (G[Q1][Q2] < 200) {
			G[Q1][Q2] += 100;
			K9++;
		}
		B9 = 1;
		G[Q1][Q2] += 10;
		Q1 = b_FNR(1); Q2 = b_FNR(1);
	}

	K7 = K9;

	if (B9 != 1) {
		s_X = "S";
		s_X0 = " ARE ";
	}

	printf( "YOUR ORDERS ARE AS FOLLOWS:\n"
	        "     DESTROY THE %d KLINGON WARSHIPS WHICH HAVE INVADED\n"
	        "   THE GALAXY BEFORE THEY CAN ATTACK FEDERATION HEADQUARTERS\n"
	        "   ON STARDATE %g   THIS GIVES YOU %g DAYS.  THERE%s\n"
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
	Z[Q1][Q2] = G[Q1][Q2];

	if (Q1 >= 1 && Q1 <= 8 && Q2 >= 1 && Q2 <= 8) {
		const char *s_G2_1 = get_quadrant_name(Q1, Q2);
		const char *s_G2_2 = get_quadrant_number(Q2);

		printf( "\n");
		if (T0 == T) {
			printf( "YOUR MISSION BEGINS WITH YOUR STARSHIP LOCATED\n"
			        "IN THE GALACTIC QUADRANT, '%s%s'.\n", s_G2_1, s_G2_2);
		} else {
			printf( "NOW ENTERING %s%s QUADRANT . . .\n", s_G2_1, s_G2_2);
		}
		printf( "\n");

		K3 = G[Q1][Q2] / 100;
		B3 = G[Q1][Q2] / 10 % 10;
		S3 = G[Q1][Q2] % 10;

		if (K3 != 0) {
			printf( "COMBAT AREA      CONDITION RED\n");

			if (S <= 200)
				printf( "   SHIELDS DANGEROUSLY LOW\n");
		}

		for (I = 1; I <= 3; I++) {
			K[I][1] = 0; K[I][2] = 0;
		}
	}

	for (I = 1; I <= 3; I++) {
		K[I][3] = 0;
	}

	for (I = 0; I < 192; I++)
		s_Q[I] = ' ';

	// POSITION ENTERPRISE IN QUADRANT, THEN PLACE "K3" KLINGONS, &
	// "B3" STARBASES, & "S3" STARS ELSEWHERE.
	set_sector(S1, S2, "<*>");

	for (I = 1; I <= K3; I++) {
		get_empty_sector();
		set_sector(R1, R2, "+K+");
		K[I][1] = R1; K[I][2] = R2;
		K[I][3] = S9 * (0.5 + b_RND(1));
	}

	if (B3 >= 1) {
		get_empty_sector();
		set_sector(R1, R2, ">!<");
		B4 = R1; B5 = R2;
	}

	for (I = 1; I <= S3; I++) {
		get_empty_sector();
		set_sector(R1, R2, " * ");
	}
}


void main_loop()
{
	while(1) {
		char s_A[4];
		int I;
		rg_t ret;

		if (S + E <= 10 || (E <= 10 && D[7] != 0)) {
			end_of_game(RG_GAME_END_NO_ENERGY);
			return;
		}

		printf( "COMMAND? ");
		b_INPUT_S(s_A, 4);

		for (I = 1; I <= 9; I++) {
			if (strnicmp(s_A, s_A1 + 3 * I - 3, 3) == 0)
				break;
		}

		ret = RG_MAIN_LOOP;

		if (I == 1) ret = course_control();
		else if (I == 2) short_range_sensors_dock();
		else if (I == 3) long_range_sensors();
		else if (I == 4) ret = phaser_control();
		else if (I == 5) ret = photon_torpedo();
		else if (I == 6) shield_control();
		else if (I == 7) damage_control();
		else if (I == 8) library_computer();
		else if (I == 9) ret = RG_GAME_END_RESIGN;
		else {
			printf( "ENTER ONE OF THE FOLLOWING:\n"
			        "  NAV  (TO SET COURSE)\n"
			        "  SRS  (FOR SHORT RANGE SENSOR SCAN)\n"
			        "  LRS  (FOR LONG RANGE SENSOR SCAN)\n"
			        "  PHA  (TO FIRE PHASERS)\n"
			        "  TOR  (TO FIRE PHOTON TORPEDOES)\n"
			        "  SHE  (TO RAISE OR LOWER SHIELDS)\n"
			        "  DAM  (FOR DAMAGE CONTROL REPORTS)\n"
			        "  COM  (TO CALL ON LIBRARY-COMPUTER)\n"
			        "  XXX  (TO RESIGN YOUR COMMAND)\n\n");
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
rg_t course_control()
{
	rg_t ret;
	double C1, // warp/torpedo course
	       W1, // warp factor
	       D6, // damage repair amount when moving
	       X1, // course X delta
	       X2, // course Y delta
	       X,  // enterprise X sector before moving
	       Y,  // enterprise Y sector before moving
	       T8; // time for warp travel

	const char *s_X;

	int N,     // energy cost/distance for navigation
	    I;     // loop variable

	bool D1;   // flag - damage control report started

	printf( "COURSE (0-9)? ");
	b_INPUT_1(&C1);

	if (C1 == 9)
		C1 = 1;

	if (C1 < 1 || C1 >= 9) {
		printf( "   LT. SULU REPORTS, 'INCORRECT COURSE DATA, SIR!'\n");
		return RG_MAIN_LOOP;
	}

	s_X = (D[1] < 0) ? "0.2" : "8";
	printf( "WARP FACTOR (0-%s)? ", s_X);
	b_INPUT_1(&W1);

	if (D[1] < 0 && W1 > 0.2) {
		printf( "WARP ENGINES ARE DAMAGED.  MAXIUM SPEED = WARP 0.2\n");
		return RG_MAIN_LOOP;
	}

	if (W1 <= 0 || W1 > 8) {
		if (W1 != 0) {
			printf( "   CHIEF ENGINEER SCOTT REPORTS 'THE ENGINES WON'T TAKE"
			        " WARP  %g !'\n", W1);
		}
		return RG_MAIN_LOOP;
	}

	N = (int)(W1 * 8 + 0.5);
	if (E - N < 0) {
		printf( "ENGINEERING REPORTS   'INSUFFICIENT ENERGY AVAILABLE\n"
		        "                       FOR MANEUVERING AT WARP %g !'\n", W1);

		if (S >= N - E && D[7] >= 0) {
			printf( "DEFLECTOR CONTROL ROOM ACKNOWLEDGES %g UNITS OF ENERGY\n"
			        "                         PRESENTLY DEPLOYED TO SHIELDS.\n", S);
		}

		return RG_MAIN_LOOP;
	}

	// KLINGONS MOVE/FIRE ON MOVING STARSHIP . . .
	for (I = 1; I <= K3; I++) {
		if (K[I][3] == 0)
			continue;

		set_sector(K[I][1], K[I][2], "   ");
		get_empty_sector();
		K[I][1] = R1; K[I][2] = R2;
		set_sector(K[I][1], K[I][2], "+K+");
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
			printf( "DAMAGE CONTROL REPORT:  ");
		}
		printf("%s%s REPAIR COMPLETED.\n", b_SPC(8),get_device_name(I));
	}

	if (b_RND(1) <= 0.2) {
		R1 = b_FNR(1);
		printf("DAMAGE CONTROL REPORT:  %s", get_device_name(R1));

		if (b_RND(1) < 0.6) {
			D[(int)(R1)] -= b_RND(1) * 5 + 1;
			printf( " DAMAGED\n\n");
		} else {
			D[(int)(R1)] += b_RND(1) * 3 + 1;
			printf( " STATE OF REPAIR IMPROVED\n\n");
		}
	}

	// BEGIN MOVING STARSHIP
	set_sector((int)(S1), (int)(S2), "   ");
	X1 = C[(int)(C1)][1] + (C[(int)(C1 + 1)][1] - C[(int)(C1)][1]) * (C1 - (int)(C1));
	X = S1; Y = S2;
	X2 = C[(int)(C1)][2] + (C[(int)(C1 + 1)][2] - C[(int)(C1)][2]) * (C1 - (int)(C1));

	for (I = 1; I <= N; I++) {
		S1 += X1; S2 += X2;

		if (S1 < 1 || S1 >= 9 || S2 < 1 || S2 >= 9) {
			if ((ret = exceeded_quadrant_limits(N, X, Y, X1, X2)) != RG_PASS)
				return ret;
			break;
		}

		S8 = (int)(S1) * 24 + (int)(S2) * 3 - 26;

		if (strncmp(s_Q + S8 - 1, "  ", 2) != 0) {
			S1 -= X1; S2 -= X2;
			printf( "WARP ENGINES SHUT DOWN AT "
			        "SECTOR %g , %g DUE TO BAD NAVAGATION\n", S1, S2);
			break;
		}
	}

	S1 = (int)(S1); S2 = (int)(S2);
	set_sector((int)(S1), (int)(S2), "<*>");
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
rg_t exceeded_quadrant_limits(int N, double X, double Y, double X1, double X2)
{
	int Q4,  // enterprise X quadrant before moving
	    Q5;  // enterprise Y quadrant before moving
	bool X5; // flag - left galaxy

	Q4 = Q1; Q5 = Q2;
	X = 8 * Q1 + X + N * X1; Y = 8 * Q2 + Y + N * X2;
	Q1 = (int)(X / 8); Q2 = (int)(Y / 8);
	S1 = (int)(X - Q1 * 8); S2 = (int)(Y - Q2 * 8);

	if (S1 == 0) {
		Q1--;
		S1 = 8;
	}

	if (S2 == 0) {
		Q2--;
		S2 = 8;
	}

	X5 = false;

	if (Q1 < 1) {
		X5 = true;
		Q1 = 1;
		S1 = 1;
	}

	if (Q1 > 8) {
		X5 = true;
		Q1 = 8;
		S1 = 8;
	}

	if (Q2 < 1) {
		X5 = true;
		Q2 = 1;
		S2 = 1;
	}

	if (Q2 > 8) {
		X5 = true;
		Q2 = 8;
		S2 = 8;
	}

	if (X5) {
		printf( "LT. UHURA REPORTS MESSAGE FROM STARFLEET COMMAND:\n"
		        "  'PERMISSION TO ATTEMPT CROSSING OF GALACTIC PERIMETER\n"
		        "  IS HEREBY *DENIED*.  SHUT DOWN YOUR ENGINES.'\n"
		        "CHIEF ENGINEER SCOTT REPORTS  'WARP ENGINES SHUT DOWN\n"
		        "  AT SECTOR %g , %g OF QUADRANT %d , %d .'\n",
			S1, S2, Q1, Q2);
		if (T > T0 + T9)
			return RG_GAME_END_TIME;
	}

	if (8 * Q1 + Q2 == 8 * Q4 + Q5)
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

	printf("SHIELD CONTROL SUPPLIES ENERGY TO COMPLETE THE MANEUVER.\n");
	S += E;
	E = 0;

	if (S <= 0)
		S = 0;
}


// LONG RANGE SENSOR SCAN CODE
void long_range_sensors()
{
	int I, // loop variable
	    J; // loop variable 2

	const char *s_O1; // horizontal rule

	if (D[3] < 0) {
		printf("LONG RANGE SENSORS ARE INOPERABLE\n");
		return;
	}

	printf("LONG RANGE SCAN FOR QUADRANT %d , %d\n", Q1, Q2);
	s_O1 = "-------------------";
	printf("%s\n", s_O1);
	for (I = Q1 - 1; I <= Q1 + 1; I++) {
		for (J = Q2 - 1; J <= Q2 + 1; J++) {
			printf(": ");
			if (I > 0 && I < 9 && J > 0 && J < 9) {
				printf("%03d ", G[I][J]);
				Z[I][J] = G[I][J];
			} else {
				printf("*** ");
			}
		}
		printf( ":\n%s\n", s_O1);
	}
}

void no_enemy_ships()
{
	printf("SCIENCE OFFICER SPOCK REPORTS  'SENSORS SHOW NO ENEMY SHIPS\n"
	       "                                IN THIS QUADRANT'\n");
}


// PHASER CONTROL CODE BEGINS HERE
rg_t phaser_control()
{
	rg_t ret;
	int I,    // loop variable
	    H1;   // phaser power divided by # of klingons in quadrant
	double X; // energy to fire

	if (D[4] < 0) {
		printf("PHASERS INOPERATIVE\n");
		return RG_MAIN_LOOP;
	}

	if (K3 <= 0) {
		no_enemy_ships();
		return RG_MAIN_LOOP;
	}

	if (D[8] < 0)
		printf("COMPUTER FAILURE HAMPERS ACCURACY\n");

	printf("PHASERS LOCKED ON TARGET;  ");

	do {
		printf("ENERGY AVAILABLE = %g UNITS\n"
		       "NUMBER OF UNITS TO FIRE? ", E);
		b_INPUT_1(&X);

		if (X <= 0)
			return RG_MAIN_LOOP;
	} while (E - X < 0);

	E -= X;
	if (D[7] < 0)
		X *= b_RND(1);

	H1 = (int)(X / K3);
	for (I = 1; I <= 3; I++) {
		int H; // phaser damage to klingon

		if (K[I][3] <= 0)
			continue;

		H = (int)(H1 / b_FND(I) * (b_RND(1) + 2));

		if (H <= 0.15 * K[I][3]) {
			printf( "SENSORS SHOW NO DAMAGE TO ENEMY AT  %g , %g\n", K[I][1], K[I][2]);
			continue;
		}

		K[I][3] -= H;
		printf( " %d UNIT HIT ON KLINGON AT SECTOR %g ,"
		        " %g\n", H, K[I][1], K[I][2]);

		if (K[I][3] > 0) {
			printf( "   (SENSORS SHOW %g UNITS REMAINING)\n", K[I][3]);
			continue;
		}

		printf( "*** KLINGON DESTROYED ***\n");
		K3--; K9--;
		set_sector(K[I][1], K[I][2], "   ");
		K[I][3] = 0;
		G[Q1][Q2] -= 100;
		Z[Q1][Q2] = G[Q1][Q2];

		if (K9 <= 0)
			return RG_GAME_END_NO_KLINGONS;
	}

	if ((ret = klingons_shooting()) != RG_PASS)
		return ret;
	return RG_MAIN_LOOP;
}


// PHOTON TORPEDO CODE BEGINS HERE
rg_t photon_torpedo()
{
	rg_t ret;
	int I,  // loop variable
	    X3, // torpedo X sector (int)
	    Y3; // torpedo Y sector (int)
	double X,  // torpedo X sector (non-int)
	       Y,  // torpedo Y sector (non-int)
	       X1, // course X delta
	       X2, // course Y delta
	       C1; // torpedo course

	if (P <= 0) {
		printf( "ALL PHOTON TORPEDOES EXPENDED\n");
		return RG_MAIN_LOOP;
	}

	if (D[5] < 0) {
		printf( "PHOTON TUBES ARE NOT OPERATIONAL\n");
		return RG_MAIN_LOOP;
	}

	while(1) {
		printf( "PHOTON TORPEDO COURSE (1-9)? ");
		b_INPUT_1(&C1);

		if (C1 == 9)
			C1 = 1;

		if (C1 < 1 || C1 >= 9) {
			printf( "ENSIGN CHEKOV REPORTS,  'INCORRECT COURSE DATA, SIR!'\n");
			return RG_MAIN_LOOP;
		}

		X1 = C[(int)(C1)][1] + (C[(int)(C1 + 1)][1] - C[(int)(C1)][1]) * (C1 - (int)(C1));
		E -= 2;
		P--;
		X2 = C[(int)(C1)][2] + (C[(int)(C1 + 1)][2] - C[(int)(C1)][2]) * (C1 - (int)(C1));
		X = S1;
		Y = S2;

		printf( "TORPEDO TRACK:\n");
		do {
			X += X1; Y += X2;
			X3 = (int)(X + 0.5); Y3 = (int)(Y + 0.5);

			if (X3 < 1 || X3 > 8 || Y3 < 1 || Y3 > 8) {
				printf( "TORPEDO MISSED\n");
				if ((ret = klingons_shooting()) != RG_PASS)
					return ret;
				return RG_MAIN_LOOP;
			}

			printf( "                %d , %d\n", X3, Y3);
		} while (is_sector(X, Y, "   "));

		if (is_sector(X, Y, "+K+")) {
			printf("*** KLINGON DESTROYED ***\n");
			K3--; K9--;

			if (K9 <= 0)
				return RG_GAME_END_NO_KLINGONS;

			for (I = 1; I <= 3; I++) {
				if (X3 == K[I][1] && Y3 == K[I][2])
					break;
			}

			if (I > 3)
				I = 3;

			K[I][3] = 0;
		} else if (is_sector(X, Y, " * ")) {
			printf( "STAR AT %d , %d ABSORBED TORPEDO ENERGY.\n", X3, Y3);

			if ((ret = klingons_shooting()) != RG_PASS)
				return ret;
			return RG_MAIN_LOOP;
		} else if (is_sector(X, Y, ">!<")) {
			printf( "*** STARBASE DESTROYED ***\n");
			B3--; B9--;

			if (B9 <= 0 && K9 <= T - T0 - T9)
				return RG_GAME_END_TORPEDOED_STARBASE;

			printf( "STARFLEET COMMAND REVIEWING YOUR RECORD TO CONSIDER\n"
			        "COURT MARTIAL!\n");
			D0 = false;
		} else {
			// if you blew up something that isn't a klingon, star, or starbase, you get to fire again?
			continue;
		}
		break;
	}

	set_sector(X, Y, "   ");
	G[Q1][Q2] = K3 * 100 + B3 * 10 + S3;
	Z[Q1][Q2] = G[Q1][Q2];

	if ((ret = klingons_shooting()) != RG_PASS)
		return ret;
	return RG_MAIN_LOOP;
}


// SHIELD CONTROL
void shield_control()
{
	double X; // desired shield strength

	if (D[7] < 0) {
		printf( "SHIELD CONTROL INOPERABLE\n");
		return;
	}

	printf( "ENERGY AVAILABLE = %g "
	        "NUMBER OF UNITS TO SHIELDS? ", E + S);
	b_INPUT_1(&X);

	if (X < 0 || S == X) {
		printf( "<SHIELDS UNCHANGED>\n");
		return;
	}

	if (X > E + S) {
		printf( "SHIELD CONTROL REPORTS  'THIS IS NOT THE FEDERATION TREASURY.'\n"
		        "<SHIELDS UNCHANGED>\n");
		return;
	}

	E += S - X;
	S = X;

	printf( "DEFLECTOR CONTROL ROOM REPORT:\n"
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

	printf( "\n");
	D3 += D4;

	if (D3 >= 1)
		D3 = 0.9;

	printf( "TECHNICIANS STANDING BY TO EFFECT REPAIRS TO YOUR SHIP;\n"
	        "ESTIMATED TIME TO REPAIR: %g STARDATES\n"
	        "WILL YOU AUTHORIZE THE REPAIR ORDER (Y/N)? ", 0.01 * (int)(100 * D3));
	b_INPUT_S(s_A, 2);

	if (strnicmp(s_A, "Y", 1) != 0)
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
	printf( "\n"
	        "DEVICE             STATE OF REPAIR\n");

	for (R1 = 1; R1 <= 8; R1++) {
		const char *s_G2 = get_device_name(R1);
		printf("%s%s %g\n", s_G2, b_SPC(25 - strlen(s_G2)), (int)(D[(int)(R1)] * 100) * 0.01);
	}

	printf( "\n");
}

// DAMAGE CONTROL
void damage_control()
{
	rg_t ret;

	if (D[6] < 0) {
		printf( "DAMAGE CONTROL REPORT NOT AVAILABLE\n");

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
		printf( "STARBASE SHIELDS PROTECT THE ENTERPRISE\n");
		return RG_PASS;
	}

	for (I = 1; I <= 3; I++) {
		int H; // phaser damage from klingon

		if (K[I][3] <= 0)
			continue;

		H = (int)((K[I][3] / b_FND(I)) * (2 + b_RND(1)));
		S -= H;
		K[I][3] /= 3 + b_RND(0);
		printf(" %d UNIT HIT ON ENTERPRISE FROM SECTOR %g , %g\n", H, K[I][1], K[I][2]);

		if (S <= 0)
			return RG_GAME_END_ENTERPRISE_DESTROYED;

		printf("      <SHIELDS DOWN TO %g UNITS>\n", S);

		if (H < 20)
			continue;

		if (b_RND(1) > 0.6 || H / S <= 0.02)
			continue;

		R1 = b_FNR(1);
		D[(int)(R1)] -= H / S + 0.5 * b_RND(1);
		printf("DAMAGE CONTROL REPORTS %s DAMAGED BY THE HIT'\n", get_device_name(R1));
	}

	return RG_PASS;
}


// END OF GAME
void end_of_game(rg_t end_type)
{
	switch (end_type)
	{
		case RG_GAME_END_NO_ENERGY:
			printf( "\n"
			        "** FATAL ERROR **   YOU'VE JUST STRANDED YOUR SHIP IN \n"
			        "SPACE\n"
			        "YOU HAVE INSUFFICIENT MANEUVERING ENERGY,"
			        " AND SHIELD CONTROL\n"
			        "IS PRESENTLY INCAPABLE OF CROSS"
			        "-CIRCUITING TO ENGINE ROOM!!\n");
			break;

		case RG_GAME_END_TORPEDOED_STARBASE:
			printf( "THAT DOES IT, CAPTAIN!!  YOU ARE HEREBY RELIEVED OF COMMAND\n"
			        "AND SENTENCED TO 99 STARDATES AT HARD LABOR ON CYGNUS 12!!\n");

		default:
			break;
	}

	switch(end_type)
	{
		case RG_GAME_END_NO_KLINGONS:
			printf( "CONGRULATION, CAPTAIN!  THEN LAST KLINGON BATTLE CRUISER\n"
			        "MENACING THE FDERATION HAS BEEN DESTROYED.\n\n"
			        "YOUR EFFICIENCY RATING IS %g\n", 1000 * (K7 / (T - T0)) * (K7 / (T - T0)));
			break;

		case RG_GAME_END_ENTERPRISE_DESTROYED:
			printf( "\n"
			        "THE ENTERPRISE HAS BEEN DESTROYED.  THEN FEDERATION "
			        "WILL BE CONQUERED\n");

		case RG_GAME_END_TIME:
		case RG_GAME_END_NO_ENERGY:
			printf( "IT IS STARDATE %g\n", T);

		case RG_GAME_END_RESIGN:
		case RG_GAME_END_TORPEDOED_STARBASE:
			printf( "THERE WERE %d KLINGON BATTLE CRUISERS LEFT AT\n"
			        "THE END OF YOUR MISSION.\n", K9);

		default:
			break;
	}

	printf( "\n\n");

	if (B9 > 0) {
		char s_A[4];
		printf( "THE FEDERATION IS IN NEED OF A NEW STARSHIP COMMANDER\n"
		        "FOR A SIMILAR MISSION -- IF THERE IS A VOLUNTEER,\n"
		        "LET HIM STEP FORWARD AND ENTER 'AYE'? ");
		b_INPUT_S(s_A, 4);

		if (strnicmp(s_A, "AYE", 3) == 0)
			return;
	}

	exit(0);
}

bool next_to_starbase()
{
	int I, J;

	for (I = S1 - 1; I <= S1 + 1; I++) {
		for (J = S2 - 1; J <= S2 + 1; J++) {
			if ((int)(I + 0.5) < 1 || (int)(I + 0.5) > 8 || (int)(J + 0.5) < 1 || (int)(J + 0.5) > 8)
				continue;

			if (is_sector(I, J, ">!<"))
				return true;
		}
	}
	return false;
}

// SHORT RANGE SENSOR SCAN & STARTUP SUBROUTINE
void short_range_sensors_dock()
{
	const char *s_C,  // alert state (GREEN, *RED*, YELLOW, DOCKED)
	           *s_O1; // horizontal rule
	int I, J;

	D0 = false;
	if (next_to_starbase()) {
		D0 = true;
		s_C = "DOCKED";
		E = E0;
		P = P0;
		printf( "SHIELDS DROPPED FOR DOCKING PURPOSES\n");
		S = 0;
	} else if (K3 > 0) {
		s_C = "*RED*";
	} else {
		s_C = "GREEN";

		if (E < E0 * 0.1)
			s_C = "YELLOW";
	}

	if (D[2] < 0) {
		printf( "\n*** SHORT RANGE SENSORS ARE OUT ***\n\n");
		return;
	}

	s_O1 = "---------------------------------";
	printf("%s\n", s_O1);
	for (I = 1; I <= 8; I++) {
		for (J = (I - 1) * 24 + 1; J <= (I - 1) * 24 + 22; J += 3) {
			printf( " %c%c%c", s_Q[J - 1], s_Q[J], s_Q[J + 1]);
		}

		if (I == 1) printf( "        STARDATE           %g\n", (int)(T * 10) * 0.1);
		if (I == 2) printf( "        CONDITION          %s\n", s_C);
		if (I == 3) printf( "        QUADRANT           %d , %d\n", Q1, Q2);
		if (I == 4) printf( "        SECTOR             %g , %g\n", S1, S2);
		if (I == 5) printf( "        PHOTON TORPEDOES   %d\n", (int)(P));
		if (I == 6) printf( "        TOTAL ENERGY       %d\n", (int)(E + S));
		if (I == 7) printf( "        SHIELDS            %d\n", (int)(S));
		if (I == 8) printf( "        KLINGONS REMAINING %d\n", (int)(K9));
	}
	printf("%s\n", s_O1);
}


// LIBRARY COMPUTER CODE
void library_computer()
{
	if (D[8] < 0) {
		printf( "COMPUTER DISABLED\n");
		return;
	}

	while(1)
	{
		double A; // library-computer function input

		printf( "COMPUTER ACTIVE AND AWAITING COMMAND? ");
		b_INPUT_1(&A);

		if (A < 0)
			return;
		printf( "\n");

		if (A == 0) galaxy_map(GALAXY_MAP_RECORD);
		else if (A == 1) status_report();
		else if (A == 2) dir_calc(DIR_CALC_KLINGONS);
		else if (A == 3) dir_calc(DIR_CALC_STARBASE);
		else if (A == 4) dir_calc(DIR_CALC_INPUT);
		else if (A == 5) galaxy_map(GALAXY_MAP_NAMES);
		else {
			printf( "FUNCTIONS AVAILABLE FROM LIBRARY-COMPUTER:\n"
			        "   0 = CUMULATIVE GALACTIC RECORD\n"
			        "   1 = STATUS REPORT\n"
			        "   2 = PHOTON TORPEDO DATA\n"
			        "   3 = STARBASE NAV DATA\n"
			        "   4 = DIRECTION/DISTANCE CALCULATOR\n"
			        "   5 = GALAXY 'REGION NAME' MAP\n\n");
			continue;
		}
		break;
	}
}


// GALAXY MAP/CUMULATIVE GALACTIC RECORD
void galaxy_map(gm_t map_type)
{
	int I, J;
	const char *s_O1; // horizontal rule

	if (map_type != GALAXY_MAP_RECORD)
	{
		// SETUP TO CHANGE CUM GAL RECORD TO GALAXY MAP
		printf( "                        THE GALAXY\n");
	} else {
		// CUM GALACTIC RECORD
		// INPUT"DO YOU WANT A HARDCOPY? IS THE TTY ON (Y/N)";A$
		// IFA$="Y"THENPOKE1229,2:POKE1237,3:NULL1
		printf( "\n"
		        "        "
		        "COMPUTER RECORD OF GALAXY FOR QUADRANT %d , %d\n"
		        "\n", Q1, Q2);
	}

	printf( "       1     2     3     4     5     6     7     8\n");
	s_O1 = "     ----- ----- ----- ----- ----- ----- ----- -----";
	printf("%s\n", s_O1);
	for (I = 1; I <= 8; I++) {
		printf( " %d ", I);
		if (map_type == GALAXY_MAP_RECORD) {
			for (J = 1; J <= 8; J++) {
				printf( "   ");

				if (Z[I][J] == 0) {
					printf( "***");
					continue;
				}

				printf("%03d", Z[I][J]);
			}
		} else {
			int J0; // spacing of names on galaxy map
			const char *s_G2; // quadrant name

			s_G2 = get_quadrant_name(I, 1);
			J0 = (int)(12 - 0.5 * strlen(s_G2));
			printf("%s%s%s", b_SPC(J0), s_G2, b_SPC(J0 + strlen(s_G2) % 2));
			s_G2 = get_quadrant_name(I, 5);
			J0 = (int)(12 - 0.5 * strlen(s_G2));
			printf("%s%s", b_SPC(J0), s_G2);
		}
		printf( "\n%s\n", s_O1);
	}
	printf( "\n");
}


// STATUS REPORT
void status_report()
{
	const char *s_X; // plural

	printf( "   STATUS REPORT:\n");

	s_X = (K9 > 1) ? "S" : "";
	printf( "KLINGON%s LEFT:  %d\n"
	        "MISSION MUST BE COMPLETED IN %g STARDATES\n",
		s_X, K9, 0.1 * (int)((T0 + T9 - T) * 10));

	if (B9 < 1) {
		printf( "YOUR STUPIDITY HAS LEFT YOU ON YOUR ON IN\n"
		        "  THE GALAXY -- YOU HAVE NO STARBASES LEFT!\n");
		damage_control();
		return;
	}

	s_X = (B9 > 1) ? "S" : "";
	printf( "THE FEDERATION IS MAINTAINING %d STARBASE%s IN THE GALAXY\n", B9, s_X);
	damage_control();
}


void actual_dir_calc(double C1, double A, double W1, double X)
{
	X = X - A; A = W1 - C1;

	C1 = (X > 0) ? ((A > 0) ? 8 : 2) : ((A > 0) ? 6 : 4);
	W1 = abs(X) - abs(A);
	W1 /= abs(X) > abs(A) ? abs(X) : abs(A);
	C1 += ((X > 0) == (A > 0)) ? W1 : -W1;

	printf( "DIRECTION = %g\n"
	        "DISTANCE = %g\n", C1, sqrt(X * X + A * A));
}


// TORPEDO, BASE NAV, D/D CALCULATOR
void dir_calc(dc_t calc_type)
{
	if (calc_type == DIR_CALC_KLINGONS)
	{
		int I;
		const char *s_X; // plural

		if (K3 <= 0) {
			no_enemy_ships();
			return;
		}

		s_X = (K3 > 1) ? "S" : "";
		printf( "FROM ENTERPRISE TO KLINGON BATTLE CRUSER%s\n", s_X);

		for (I = 1; I <= 3; I++) {
			if (K[I][3] <= 0)
				continue;

			actual_dir_calc(S1, S2, K[I][1], K[I][2]);
		}
	} else if (calc_type == DIR_CALC_STARBASE) {
		if (B3 == 0) {
			printf( "MR. SPOCK REPORTS,  'SENSORS SHOW NO STARBASES IN THIS"
			        " QUADRANT.'\n");
			return;
		}

		printf( "FROM ENTERPRISE TO STARBASE:\n");
		actual_dir_calc(S1, S2, B4, B5);
	} else if (calc_type == DIR_CALC_INPUT) {
		double C1, // initial X coord
		       A,  // initial Y coord
		       W1, // final X coord
		       X;  // final Y coord

		printf( "DIRECTION/DISTANCE CALCULATOR:\n"
		        "YOU ARE AT QUADRANT  %d , %d  SECTOR  %g , %g\n"
		        "PLEASE ENTER\n"
		        "  INITIAL COORDINATES (X,Y)? ", Q1, Q2, S1, S2);
		b_INPUT_2(&C1, &A);
		printf( "  FINAL COORDINATES (X,Y)? ");
		b_INPUT_2(&W1, &X);
		actual_dir_calc(C1, A, W1, X);
	}
}


// FIND EMPTY PLACE IN QUADRANT (FOR THINGS)
void get_empty_sector()
{
	do {
		R1 = b_FNR(1); R2 = b_FNR(1);
	} while (!is_sector(R1, R2, "   "));
}


// INSERT IN STRING ARRAY FOR QUADRANT
void set_sector(double Z1, double Z2, const char *s_A)
{
	S8 = (int)(Z2 - 0.5) * 3 + (int)(Z1 - 0.5) * 24;

	if (strlen(s_A) != 3) {
		printf( "ERROR\n");
		exit(0);
	}

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
bool is_sector(double Z1, double Z2, const char *s_A)
{
	Z1 = (int)(Z1 + 0.5); Z2 = (int)(Z2 + 0.5);
	S8 = (Z2 - 1) * 3 + (Z1 - 1) * 24 + 1;
	return (strncmp(s_Q + S8 - 1, s_A, 3) == 0);
}


// QUADRANT NAME IN G2$ FROM Z4,Z5 (=Q1,Q2)
const char *get_quadrant_name(int Z4, int Z5)
{
	const char *quadrant_names[] = {
		"ANTARES", "RIGEL", "PROCYON", "VEGA", "CANOPUS", "ALTAIR",
		"SAGITTARIUS", "POLLUX", "SIRIUS", "DENEB", "CAPELLA",
		"BETELGEUSE", "ALDEBARAN", "REGULUS", "ARCTURUS", "SPICA"
	};

	if (Z4 >= 1 && Z4 <= 8)
		return quadrant_names[Z4 + ((Z5 <= 4) ? -1 : 7)];
	return quadrant_names[8];
}

const char *get_quadrant_number(int Z5)
{
	const char *quadrant_numbers[] = {
		" I", " II", " III", " IV"
	};

	if (Z5 >= 1 && Z5 <= 8)
		return quadrant_numbers[(int)(Z5 - 1) % 4];
	return "";
}

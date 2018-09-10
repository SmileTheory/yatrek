#include <iostream>
#include <string>
#include <cmath>
#include <ctime>

using namespace std;

////////////////////////////////////////////////////////////////////////////////
// below from "A small noncryptographic PRNG"
// http://burtleburtle.net/bob/rand/smallprng.html

typedef unsigned long int u4;
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

string b_LEFT(string s, int len)
{
	return s.substr(0, len);
}

string b_MID(string s, int pos, int len)
{
	return s.substr(pos - 1, len);
}

string b_RIGHT(string s, int len)
{
	return s.substr(s.length() - len, len);
}

int b_LEN(string s)
{
	return s.length();
}

string b_TAB(int len)
{
	string out = "";

	while (len--)
		out += " ";
	return out;
}

string b_STR(int num)
{
	return to_string(num);
}

/*
 * int variables notes:
 *
 * E can be non-int, because there is no check for firing phasers at non-int
 * strength.
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

int    G[9][9],  // galaxy info: 100 * klingons + 10 * starbases + stars
       C[10][3], // course calc constants
       K[4][4],  // klingon info: [1] = x [2] = y [3] = power
       aN[4],    // long range scan info, same format as G[][]
       Z[9][9];  // known galaxy info, same format as G[][]

double D[9];     // devices state of repair, < 0 -> damaged

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

       D3, // estimated starbase repair time
       D4, // random extra starbase repair time

       C1, // warp/torpedo course
           // direction/distance calculator: initial X coord
       A,  // direction/distance calculator: initial Y coord
       W1, // warp factor
           // direction/distance calculator: final X coord
       X,  // enterprise X sector before moving
           // later absolute X coord in galaxy when leaving quadrant
	   // phaser control(): energy to fire
           // direction/distance calculator: final Y coord
       Y,  // enterprise Y sector before moving
           // later absolute Y coord in galaxy when leaving quadrant
       X1, // course X delta
       X2, // course Y delta
       T8, // time for warp travel
       D6, // damage repair amount when moving

       Z1, // set/get sector(): X parameter
       Z2; // set/get sector(): Y parameter


int    I,  // loop variable
       J,  // loop variable 2
       L,  // loop variable 3
       N,  // energy cost/distance for navigation

       Q1, // enterprise X quadrant
       Q2, // enterprise Y quadrant
       Q4, // enterprise X quadrant before moving
       Q5, // enterprise Y quadrant before moving

       P,  // current # of torpedoes
       P0, // max # of torpedoes
       X3, // torpedo X sector
       Y3, // torpedo Y sector

       K3, // # of klingons in quadrant
       K7, // starting # of klingons in galaxy
       K9, // current # of klingons in galaxy

       B3, // # of starbases in quadrant
       B4, // starbase X sector
       B5, // starbase Y sector
       B9, // current # of starbases in galaxy

       S3, // # of stars in quadrant

       S8, // position in quadrant string array

       H1, // phaser power divided by # of klingons in quadrant
       H,  // phaser damage to klingon

       J0, // spacing of names on galaxy map

       Z3, // get sector(): match result
       Z4, // quadrant name(): X parameter
       Z5, // quadrant name(): Y parameter

       D0, // flag - docked
       D1, // flag - damage control report started
       G5, // quadrant name (): flag - region name only
       H8, // flag: region names only on galaxy map
           // direction/distance calculator: flag - don't loop through klingons
       X5; // flag - left galaxy


string s_Z,  // 25 space constant, for spacing galaxy map names and making quadrant string
       s_X,  // plural "S" and maximum warp factor
       s_X0, // " is " or " are "
       s_A1, // list of commands constant
       s_G2, // quadrant/device (): name result
       s_Q,  // quadrant string
       s_A,  // command input
             // get/set sector (): object parameter
             // damage report routine: authorize repair input
             // game over routine: another game input
       s_O1, // horizontal rules
       s_C;  // alert state (GREEN, *RED*, YELLOW, DOCKED)

double b_FND(double D)
{
	return sqrt((K[I][1] - S1) * (K[I][1] - S1) + (K[I][2] - S2) * (K[I][2] - S2));
}

double b_FNR(double R)
{
	return (int)(b_RND(R) * 7.98 + 1.01);
}

typedef enum {
	RG_PASS	= 0,
	RG_L_1320,
	RG_L_1980,
	RG_L_1990,
	RG_L_6220,
	RG_L_6240,
	RG_L_6270,
	RG_L_6370
} rg_t;

rg_t GOTO_L_2300();
void GOSUB_L_3910();
rg_t GOTO_L_4000();
rg_t GOTO_L_4260();
rg_t GOTO_L_4700();
rg_t GOTO_L_5530();
rg_t GOTO_L_5690();
rg_t GOSUB_L_6000();
void GOTO_L_6220(rg_t ret);
void GOSUB_L_6430();
rg_t GOTO_L_7290();
void GOSUB_L_8590();
void GOSUB_L_8670();
void GOSUB_L_8790();
void GOSUB_L_8830();
void GOSUB_L_9030();

int main(int argc, char *argv[])
{
	rg_t ret;

	raninit(&rstate, argc == 2 ? atoi(argv[1]) : time(NULL));
L_10:
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
	cout << "\n\n\n\n\n\n\n\n\n\n\n";
	cout << "                                    ,------*------,\n";
	cout << "                    ,-------------   '---  ------'\n";
	cout << "                     '-------- --'      / /\n";
	cout << "                         ,---' '-------/ /--,\n";
	cout << "                          '----------------'\n\n";
	cout << "                    THE USS ENTERPRISE --- NCC-1701\n";
	cout << "\n\n\n\n\n";
// CLEAR 600
	s_Z = "                         ";
	T = (int)(b_RND(1) * 20 + 20) * 100; T0 = T; T9 = 25 + (int)(b_RND(1) * 10); D0 = 0; E = 3000; E0 = E;
	P = 10; P0 = P; S9 = 200; S = 0; B9 = 0; K9 = 0; s_X = ""; s_X0 = " IS ";
// FIXME-DEF b_FND(D)=sqrt((K[I][1]-S1)/* FIXME-POWER */2+(K[I][2]-S2)/* FIXME-POWER */2);
// FIXME-DEF b_FNR(R)=(int)(b_RND(R)*7.98+1.01);
// INITIALIZE ENTERPRIZE'S POSITION
	Q1 = b_FNR(1); Q2 = b_FNR(1); S1 = b_FNR(1); S2 = b_FNR(1);
	for (I = 1; I <= 9; I++) {
		C[I][1] = 0; C[I][2] = 0;
	}
	C[3][1] = -1; C[2][1] = -1; C[4][1] = -1; C[4][2] = -1; C[5][2] = -1; C[6][2] = -1;
	C[1][2] = 1; C[2][2] = 1; C[6][1] = 1; C[7][1] = 1; C[8][1] = 1; C[8][2] = 1; C[9][2] = 1;
	for (I = 1; I <= 8; I++) {
		D[I] = 0;
	}
	s_A1 = "NAVSRSLRSPHATORSHEDAMCOMXXX";
// SETUP WHAT EXHISTS IN GALAXY . . .
// K3= # KLINGONS  B3= # STARBASES  S3 = # STARS
	for (I = 1; I <= 8; I++) {
		for (J = 1; J <= 8; J++) {
			K3 = 0; Z[I][J] = 0; R1 = b_RND(1);
			if (R1 > 0.98) {
				K3 = 3; K9 = K9 + 3; goto L_980;
			}
			if (R1 > 0.95) {
				K3 = 2; K9 = K9 + 2; goto L_980;
			}
			if (R1 > 0.8) {
				K3 = 1; K9 = K9 + 1;
			}
L_980:
			B3 = 0; if (b_RND(1) > 0.96) {
				B3 = 1; B9 = B9 + 1;
			}
			G[I][J] = K3 * 100 + B3 * 10 + b_FNR(1);
		}
	}
	if (K9 > T9) T9 = K9 + 1;
	if (B9 != 0) goto L_1200;
	if (G[Q1][Q2] < 200) {
		G[Q1][Q2] = G[Q1][Q2] + 100; K9 = K9 + 1;
	}
	B9 = 1; G[Q1][Q2] = G[Q1][Q2] + 10; Q1 = b_FNR(1); Q2 = b_FNR(1);
L_1200:
	K7 = K9; if (B9 != 1) {
		s_X = "S"; s_X0 = " ARE ";
	}
	cout << "YOUR ORDERS ARE AS FOLLOWS:\n";
	cout << "     DESTROY THE " << K9 << " KLINGON WARSHIPS WHICH HAVE INVADED\n";
	cout << "   THE GALAXY BEFORE THEY CAN ATTACK FEDERATION HEADQUARTERS\n";
	cout << "   ON STARDATE " << T0 + T9 << "   THIS GIVES YOU " << T9 << " DAYS.  THERE" << s_X0 << "\n";
	cout << "   " << B9 << " STARBASE" << s_X << " IN THE GALAXY FOR RESUPPLYING YOUR SHIP\n";
	cout << "\n";   // PRINT"HIT ANY KEY EXCEPT RETURN WHEN READY TO ACCEPT COMMAND"
	I = b_RND(1);   // IF INP(1)=13 THEN 1300
// HERE ANY TIME NEW QUADRANT ENTERED
L_1320:
	Z4 = Q1; Z5 = Q2; K3 = 0; B3 = 0; S3 = 0; G5 = 0; D4 = 0.5 * b_RND(1); Z[Q1][Q2] = G[Q1][Q2];
	if (Q1 < 1 || Q1 > 8 || Q2 < 1 || Q2 > 8) goto L_1600;
	GOSUB_L_9030(); cout << "\n"; if (T0 != T) goto L_1490;
	cout << "YOUR MISSION BEGINS WITH YOUR STARSHIP LOCATED\n";
	cout << "IN THE GALACTIC QUADRANT, '" << s_G2 << "'.\n"; goto L_1500;
L_1490:
	cout << "NOW ENTERING " << s_G2 << " QUADRANT . . .\n";
L_1500:
	cout << "\n"; K3 = (int)(G[Q1][Q2] * 0.01); B3 = (int)(G[Q1][Q2] * 0.1) - 10 * K3;
	S3 = G[Q1][Q2] - 100 * K3 - 10 * B3; if (K3 == 0) goto L_1590;
	cout << "COMBAT AREA      CONDITION RED\n"; if (S > 200) goto L_1590;
	cout << "   SHIELDS DANGEROUSLY LOW\n";
L_1590:
	for (I = 1; I <= 3; I++) {
		K[I][1] = 0; K[I][2] = 0;
	}
L_1600:
	for (I = 1; I <= 3; I++) {
		K[I][3] = 0;
	}
	s_Q = s_Z + s_Z + s_Z + s_Z + s_Z + s_Z + s_Z + b_LEFT(s_Z, 17);
// POSITION ENTERPRISE IN QUADRANT, THEN PLACE "K3" KLINGONS, &
// "B3" STARBASES, & "S3" STARS ELSEWHERE.
	s_A = "<*>"; Z1 = S1; Z2 = S2; GOSUB_L_8670(); if (K3 < 1) goto L_1820;
	for (I = 1; I <= K3; I++) {
		GOSUB_L_8590(); s_A = "+K+"; Z1 = R1; Z2 = R2;
		GOSUB_L_8670(); K[I][1] = R1; K[I][2] = R2; K[I][3] = S9 * (0.5 + b_RND(1));
	}
L_1820:
	if (B3 < 1) goto L_1910;
	GOSUB_L_8590(); s_A = ">!<"; Z1 = R1; B4 = R1; Z2 = R2; B5 = R2; GOSUB_L_8670();
L_1910:
	for (I = 1; I <= S3; I++) {
		GOSUB_L_8590(); s_A = " * "; Z1 = R1; Z2 = R2; GOSUB_L_8670();
	}
L_1980:
	GOSUB_L_6430();
L_1990:
	if (S + E > 10) {
		if (E > 10 || D[7] == 0) goto L_2060;
	}
	cout << "\n"; cout << "** FATAL ERROR **   YOU'VE JUST STRANDED YOUR SHIP IN \n";
	cout << "SPACE\n"; cout << "YOU HAVE INSUFFICIENT MANEUVERING ENERGY,";
	cout << " AND SHIELD CONTROL\n"; cout << "IS PRESENTLY INCAPABLE OF CROSS";
	cout << "-CIRCUITING TO ENGINE ROOM!!\n"; GOTO_L_6220(RG_L_6220); goto L_10;
L_2060:
	cout << "COMMAND"; cout << "? "; cin >> s_A;
	ret = RG_PASS;
	for (I = 1; I <= 9; I++) {
		if (b_LEFT(s_A, 3) != b_MID(s_A1, 3 * I - 2, 3)) goto L_2160;
		if (I == 1) ret = GOTO_L_2300();
		else if (I == 2) ret = RG_L_1980;
		else if (I == 3) ret = GOTO_L_4000();
		else if (I == 4) ret = GOTO_L_4260();
		else if (I == 5) ret = GOTO_L_4700();
		else if (I == 6) ret = GOTO_L_5530();
		else if (I == 7) ret = GOTO_L_5690();
		else if (I == 8) ret = GOTO_L_7290();
		else if (I == 9) ret = RG_L_6270;
		break;
L_2160:
		;
	}

	if (ret == RG_L_1320)
		goto L_1320;
	else if (ret == RG_L_1980)
		goto L_1980;
	else if (ret == RG_L_1990)
		goto L_1990;
	else if (ret != 0)
	{
		GOTO_L_6220(ret);
		goto L_10;
	}

	cout << "ENTER ONE OF THE FOLLOWING:\n";
	cout << "  NAV  (TO SET COURSE)\n";
	cout << "  SRS  (FOR SHORT RANGE SENSOR SCAN)\n";
	cout << "  LRS  (FOR LONG RANGE SENSOR SCAN)\n";
	cout << "  PHA  (TO FIRE PHASERS)\n";
	cout << "  TOR  (TO FIRE PHOTON TORPEDOES)\n";
	cout << "  SHE  (TO RAISE OR LOWER SHIELDS)\n";
	cout << "  DAM  (FOR DAMAGE CONTROL REPORTS)\n";
	cout << "  COM  (TO CALL ON LIBRARY-COMPUTER)\n";
	cout << "  XXX  (TO RESIGN YOUR COMMAND)\n\n"; goto L_1990;
}

// COURSE CONTROL BEGINS HERE
rg_t GOTO_L_2300()
{
	cout << "COURSE (0-9)"; cout << "? "; cin >> C1; if (C1 == 9) C1 = 1;
	if (C1 >= 1 && C1 < 9) goto L_2350;
	cout << "   LT. SULU REPORTS, 'INCORRECT COURSE DATA, SIR!'\n"; return RG_L_1990;
L_2350:
	s_X = "8"; if (D[1] < 0) s_X = "0.2";
	cout << "WARP FACTOR (0-" << s_X << ")"; cout << "? "; cin >> W1; if (D[1] < 0 && W1 > 0.2) goto L_2470;
	if (W1 > 0 && W1 <= 8) goto L_2490;
	if (W1 == 0) return RG_L_1990;
	cout << "   CHIEF ENGINEER SCOTT REPORTS 'THE ENGINES WON'T TAKE";
	cout << " WARP  " << W1 << " !'\n"; return RG_L_1990;
L_2470:
	cout << "WARP ENGINES ARE DAMAGED.  MAXIUM SPEED = WARP 0.2\n"; return RG_L_1990;
L_2490:
	N = (int)(W1 * 8 + 0.5); if (E - N >= 0) goto L_2590;
	cout << "ENGINEERING REPORTS   'INSUFFICIENT ENERGY AVAILABLE\n";
	cout << "                       FOR MANEUVERING AT WARP " << W1 << " !'\n";
	if (S < N - E || D[7] < 0) return RG_L_1990;
	cout << "DEFLECTOR CONTROL ROOM ACKNOWLEDGES " << S << " UNITS OF ENERGY\n";
	cout << "                         PRESENTLY DEPLOYED TO SHIELDS.\n";
	return RG_L_1990;
// KLINGONS MOVE/FIRE ON MOVING STARSHIP . . .
L_2590:
	for (I = 1; I <= K3; I++) {
		if (K[I][3] == 0) goto L_2700;
		s_A = "   "; Z1 = K[I][1]; Z2 = K[I][2]; GOSUB_L_8670(); GOSUB_L_8590();
		K[I][1] = Z1; K[I][2] = Z2; s_A = "+K+"; GOSUB_L_8670();
L_2700:
		;
	}
	if (GOSUB_L_6000() == RG_L_6240) return RG_L_6240;
	D1 = 0; D6 = W1; if (W1 >= 1) D6 = 1;
	for (I = 1; I <= 8; I++) {
		if (D[I] >= 0) goto L_2880;
		D[I] = D[I] + D6; if (D[I] > -0.1 && D[I] < 0) {
			D[I] = -0.1; goto L_2880;
		}
		if (D[I] < 0) goto L_2880;
		if (D1 != 1) {
			D1 = 1; cout << "DAMAGE CONTROL REPORT:  ";
		}
		cout << b_TAB(8); R1 = I; GOSUB_L_8790(); cout << s_G2 << " REPAIR COMPLETED.\n";
L_2880:
		;
	}
	if (b_RND(1) > 0.2) goto L_3070;
	R1 = b_FNR(1); if (b_RND(1) >= 0.6) goto L_3000;
	D[(int)(R1)] = D[(int)(R1)] - (b_RND(1) * 5 + 1); cout << "DAMAGE CONTROL REPORT:  ";
	GOSUB_L_8790(); cout << s_G2 << " DAMAGED\n\n"; goto L_3070;
L_3000:
	D[(int)(R1)] = D[(int)(R1)] + b_RND(1) * 3 + 1; cout << "DAMAGE CONTROL REPORT:  ";
	GOSUB_L_8790(); cout << s_G2 << " STATE OF REPAIR IMPROVED\n\n";
// BEGIN MOVING STARSHIP
L_3070:
	s_A = "   "; Z1 = (int)(S1); Z2 = (int)(S2); GOSUB_L_8670();
	X1 = C[(int)(C1)][1] + (C[(int)(C1 + 1)][1] - C[(int)(C1)][1]) * (C1 - (int)(C1)); X = S1; Y = S2;
	X2 = C[(int)(C1)][2] + (C[(int)(C1 + 1)][2] - C[(int)(C1)][2]) * (C1 - (int)(C1)); Q4 = Q1; Q5 = Q2;
	for (I = 1; I <= N; I++) {
		S1 = S1 + X1; S2 = S2 + X2; if (S1 < 1 || S1 >= 9 || S2 < 1 || S2 >= 9) goto L_3500;
		S8 = (int)(S1) * 24 + (int)(S2) * 3 - 26; if (b_MID(s_Q, S8, 2) == "  ") goto L_3360;
		S1 = (int)(S1 - X1); S2 = (int)(S2 - X2); cout << "WARP ENGINES SHUT DOWN AT ";
		cout << "SECTOR " << S1 << " , " << S2 << " DUE TO BAD NAVAGATION\n"; goto L_3370;
L_3360:
		;
	}
	S1 = (int)(S1); S2 = (int)(S2);
L_3370:
	s_A = "<*>"; Z1 = (int)(S1); Z2 = (int)(S2); GOSUB_L_8670(); GOSUB_L_3910(); T8 = 1;
	if (W1 < 1) T8 = 0.1 * (int)(10 * W1);
	T = T + T8; if (T > T0 + T9) return RG_L_6220;
	// SEE IF DOCKED, THEN GET COMMAND
	return RG_L_1980;
// EXCEEDED QUADRANT LIMITS
L_3500:
	X = 8 * Q1 + X + N * X1; Y = 8 * Q2 + Y + N * X2; Q1 = (int)(X / 8); Q2 = (int)(Y / 8); S1 = (int)(X - Q1 * 8);
	S2 = (int)(Y - Q2 * 8); if (S1 == 0) {
		Q1 = Q1 - 1; S1 = 8;
	}
	if (S2 == 0) {
		Q2 = Q2 - 1; S2 = 8;
	}
	X5 = 0; if (Q1 < 1) {
		X5 = 1; Q1 = 1; S1 = 1;
	}
	if (Q1 > 8) {
		X5 = 1; Q1 = 8; S1 = 8;
	}
	if (Q2 < 1) {
		X5 = 1; Q2 = 1; S2 = 1;
	}
	if (Q2 > 8) {
		X5 = 1; Q2 = 8; S2 = 8;
	}
	if (X5 == 0) goto L_3860;
	cout << "LT. UHURA REPORTS MESSAGE FROM STARFLEET COMMAND:\n";
	cout << "  'PERMISSION TO ATTEMPT CROSSING OF GALACTIC PERIMETER\n";
	cout << "  IS HEREBY *DENIED*.  SHUT DOWN YOUR ENGINES.'\n";
	cout << "CHIEF ENGINEER SCOTT REPORTS  'WARP ENGINES SHUT DOWN\n";
	cout << "  AT SECTOR " << S1 << " , " << S2 << " OF QUADRANT " << Q1 << " , " << Q2 << " .'\n";
	if (T > T0 + T9) return RG_L_6220;
L_3860:
	if (8 * Q1 + Q2 == 8 * Q4 + Q5) goto L_3370;
	T = T + 1; GOSUB_L_3910(); return RG_L_1320;
}

// MANEUVER ENERGY S/R **
void GOSUB_L_3910()
{
	E = E - N - 10; if (E >= 0) return;
	cout << "SHIELD CONTROL SUPPLIES ENERGY TO COMPLETE THE MANEUVER.\n";
	S = S + E; E = 0; if (S <= 0) S = 0;
	return;
}

// LONG RANGE SENSOR SCAN CODE
rg_t GOTO_L_4000()
{
	if (D[3] < 0) {
		cout << "LONG RANGE SENSORS ARE INOPERABLE\n"; return RG_L_1990;
	}
	cout << "LONG RANGE SCAN FOR QUADRANT " << Q1 << " , " << Q2 << "\n";
	s_O1 = "-------------------"; cout << s_O1 << "\n";
	for (I = Q1 - 1; I <= Q1 + 1; I++) {
		aN[1] = -1; aN[2] = -2; aN[3] = -3;
		for (J = Q2 - 1; J <= Q2 + 1; J++) {
			if (I > 0 && I < 9 && J > 0 && J < 9) {
				aN[J - Q2 + 2] = G[I][J]; Z[I][J] = G[I][J];
			}
		}
		for (L = 1; L <= 3; L++) {
			cout << ": "; if (aN[L] < 0) {
				cout << "*** "; goto L_4230;
			}
			cout << b_RIGHT(b_STR(aN[L] + 1000), 3) << " ";
L_4230:
			;
		}
		cout << ":\n"; cout << s_O1 << "\n";
	}
	return RG_L_1990;
}

rg_t GOTO_L_4270()
{
	cout << "SCIENCE OFFICER SPOCK REPORTS  'SENSORS SHOW NO ENEMY SHIPS\n";
	cout << "                                IN THIS QUADRANT'\n"; return RG_L_1990;
}

// PHASER CONTROL CODE BEGINS HERE
rg_t GOTO_L_4260()
{
	if (D[4] < 0) {
		cout << "PHASERS INOPERATIVE\n"; return RG_L_1990;
	}
	if (K3 > 0) goto L_4330;
	return GOTO_L_4270();
L_4330:
	if (D[8] < 0) cout << "COMPUTER FAILURE HAMPERS ACCURACY\n";
	cout << "PHASERS LOCKED ON TARGET;  ";
L_4360:
	cout << "ENERGY AVAILABLE = " << E << " UNITS\n";
	cout << "NUMBER OF UNITS TO FIRE"; cout << "? "; cin >> X; if (X <= 0) return RG_L_1990;
	if (E - X < 0) goto L_4360;
	E = E - X; if (D[7] < 0) X = X * b_RND(1);
	H1 = (int)(X / K3);
	for (I = 1; I <= 3; I++) {
		if (K[I][3] <= 0) goto L_4670;
		H = (int)((H1 / b_FND(0)) * (b_RND(1) + 2)); if (H > 0.15 * K[I][3]) goto L_4530;
		cout << "SENSORS SHOW NO DAMAGE TO ENEMY AT  " << K[I][1] << " , " << K[I][2] << "\n"; goto L_4670;
L_4530:
		K[I][3] = K[I][3] - H; cout << " " << H << " UNIT HIT ON KLINGON AT SECTOR " << K[I][1] << " ,";
		cout << " " << K[I][2] << "\n"; if (K[I][3] <= 0) {
			cout << "*** KLINGON DESTROYED ***\n"; goto L_4580;
		}
		cout << "   (SENSORS SHOW " << K[I][3] << " UNITS REMAINING)\n"; goto L_4670;
L_4580:
		K3 = K3 - 1; K9 = K9 - 1; Z1 = K[I][1]; Z2 = K[I][2]; s_A = "   "; GOSUB_L_8670();
		K[I][3] = 0; G[Q1][Q2] = G[Q1][Q2] - 100; Z[Q1][Q2] = G[Q1][Q2]; if (K9 <= 0) return RG_L_6370;
L_4670:
		;
	}
	if (GOSUB_L_6000() == RG_L_6240) return RG_L_6240;
	return RG_L_1990;
}

// PHOTON TORPEDO CODE BEGINS HERE
rg_t GOTO_L_4700()
{
	if (P <= 0) {
		cout << "ALL PHOTON TORPEDOES EXPENDED\n"; return RG_L_1990;
	}
	if (D[5] < 0) {
		cout << "PHOTON TUBES ARE NOT OPERATIONAL\n"; return RG_L_1990;
	}
L_4760:
	cout << "PHOTON TORPEDO COURSE (1-9)"; cout << "? "; cin >> C1; if (C1 == 9) C1 = 1;
	if (C1 >= 1 && C1 < 9) goto L_4850;
	cout << "ENSIGN CHEKOV REPORTS,  'INCORRECT COURSE DATA, SIR!'\n";
	return RG_L_1990;
L_4850:
	X1 = C[(int)(C1)][1] + (C[(int)(C1 + 1)][1] - C[(int)(C1)][1]) * (C1 - (int)(C1)); E = E - 2; P = P - 1;
	X2 = C[(int)(C1)][2] + (C[(int)(C1 + 1)][2] - C[(int)(C1)][2]) * (C1 - (int)(C1)); X = S1; Y = S2;
	cout << "TORPEDO TRACK:\n";
L_4920:
	X = X + X1; Y = Y + X2; X3 = (int)(X + 0.5); Y3 = (int)(Y + 0.5);
	if (X3 < 1 || X3 > 8 || Y3 < 1 || Y3 > 8) goto L_5490;
	cout << "                " << X3 << " , " << Y3 << "\n"; s_A = "   "; Z1 = X; Z2 = Y; GOSUB_L_8830();
	if (Z3 != 0) goto L_4920;
	s_A = "+K+"; Z1 = X; Z2 = Y; GOSUB_L_8830(); if (Z3 == 0) goto L_5210;
	cout << "*** KLINGON DESTROYED ***\n"; K3 = K3 - 1; K9 = K9 - 1; if (K9 <= 0) return RG_L_6370;
	for (I = 1; I <= 3; I++) {
		if (X3 == K[I][1] && Y3 == K[I][2]) goto L_5190;
	}
	I = 3;
L_5190:
	K[I][3] = 0; goto L_5430;
L_5210:
	s_A = " * "; Z1 = X; Z2 = Y; GOSUB_L_8830(); if (Z3 == 0) goto L_5280;
	cout << "STAR AT " << X3 << " , " << Y3 << " ABSORBED TORPEDO ENERGY.\n"; if (GOSUB_L_6000() == RG_L_6240) return RG_L_6240;
	return RG_L_1990;
L_5280:
	s_A = ">!<"; Z1 = X; Z2 = Y; GOSUB_L_8830(); if (Z3 == 0) goto L_4760;
	cout << "*** STARBASE DESTROYED ***\n"; B3 = B3 - 1; B9 = B9 - 1;
	if (B9 > 0 || K9 > T - T0 - T9) goto L_5400;
	cout << "THAT DOES IT, CAPTAIN!!  YOU ARE HEREBY RELIEVED OF COMMAND\n";
	cout << "AND SENTENCED TO 99 STARDATES AT HARD LABOR ON CYGNUS 12!!\n";
	return RG_L_6270;
L_5400:
	cout << "STARFLEET COMMAND REVIEWING YOUR RECORD TO CONSIDER\n";
	cout << "COURT MARTIAL!\n"; D0 = 0;
L_5430:
	Z1 = X; Z2 = Y; s_A = "   "; GOSUB_L_8670();
	G[Q1][Q2] = K3 * 100 + B3 * 10 + S3; Z[Q1][Q2] = G[Q1][Q2]; if (GOSUB_L_6000() == RG_L_6240) return RG_L_6240;
	return RG_L_1990;
L_5490:
	cout << "TORPEDO MISSED\n"; if (GOSUB_L_6000() == RG_L_6240) return RG_L_6240;
	return RG_L_1990;
}

// SHIELD CONTROL
rg_t GOTO_L_5530()
{
	if (D[7] < 0) {
		cout << "SHIELD CONTROL INOPERABLE\n"; return RG_L_1990;
	}
	cout << "ENERGY AVAILABLE = " << E + S << " "; cout << "NUMBER OF UNITS TO SHIELDS"; cout << "? "; cin >> X;
	if (X < 0 || S == X) {
		cout << "<SHIELDS UNCHANGED>\n"; return RG_L_1990;
	}
	if (X <= E + S) goto L_5630;
	cout << "SHIELD CONTROL REPORTS  'THIS IS NOT THE FEDERATION TREASURY.'\n";
	cout << "<SHIELDS UNCHANGED>\n"; return RG_L_1990;
L_5630:
	E = E + S - X; S = X; cout << "DEFLECTOR CONTROL ROOM REPORT:\n";
	cout << "  'SHIELDS NOW AT " << (int)(S) << " UNITS PER YOUR COMMAND.'\n"; return RG_L_1990;
}

// DAMAGE CONTROL
rg_t GOTO_L_5690()
{
	if (D[6] >= 0) goto L_5910;
	cout << "DAMAGE CONTROL REPORT NOT AVAILABLE\n"; if (D0 == 0) return RG_L_1990;
L_5720:
	D3 = 0;
	for (I = 1; I <= 8; I++) {
		if (D[I] < 0) D3 = D3 + 0.1;
	}
	if (D3 == 0) return RG_L_1990;
	cout << "\n"; D3 = D3 + D4; if (D3 >= 1) D3 = 0.9;
	cout << "TECHNICIANS STANDING BY TO EFFECT REPAIRS TO YOUR SHIP;\n";
	cout << "ESTIMATED TIME TO REPAIR: " << 0.01 * (int)(100 * D3) << " STARDATES\n";
	cout << "WILL YOU AUTHORIZE THE REPAIR ORDER (Y/N)"; cout << "? "; cin >> s_A;
	if (s_A != "Y") return RG_L_1990;
	for (I = 1; I <= 8; I++) {
		if (D[I] < 0) D[I] = 0;
	}
	T = T + D3 + 0.1;
L_5910:
	cout << "\n"; cout << "DEVICE             STATE OF REPAIR\n";
	for (R1 = 1; R1 <= 8; R1++) {
		GOSUB_L_8790(); cout << s_G2 << b_LEFT(s_Z, 25 - b_LEN(s_G2)) << " " << (int)(D[(int)(R1)] * 100) * 0.01 << "\n";
	}
	cout << "\n"; if (D0 != 0) goto L_5720;
	return RG_L_1990;
}

// KLINGONS SHOOTING
rg_t GOSUB_L_6000()
{
	if (K3 <= 0) return RG_PASS;
	if (D0 != 0) {
		cout << "STARBASE SHIELDS PROTECT THE ENTERPRISE\n"; return RG_PASS;
	}
	for (I = 1; I <= 3; I++) {
		if (K[I][3] <= 0) goto L_6200;
		H = (int)((K[I][3] / b_FND(1)) * (2 + b_RND(1))); S = S - H; K[I][3] = K[I][3] / (3 + b_RND(0));
		cout << " " << H << " UNIT HIT ON ENTERPRISE FROM SECTOR " << K[I][1] << " , " << K[I][2] << "\n";
		if (S <= 0) return RG_L_6240;
		cout << "      <SHIELDS DOWN TO " << S << " UNITS>\n"; if (H < 20) goto L_6200;
		if (b_RND(1) > 0.6 || H / S <= 0.02) goto L_6200;
		R1 = b_FNR(1); D[(int)(R1)] = D[(int)(R1)] - H / S - 0.5 * b_RND(1); GOSUB_L_8790();
		cout << "DAMAGE CONTROL REPORTS " << s_G2 << " DAMAGED BY THE HIT'\n";
L_6200:
		;
	}
	return RG_PASS;
}

// END OF GAME
void GOTO_L_6220(rg_t ret)
{
	if (ret == RG_L_6240)
		goto L_6240;
	else if (ret == RG_L_6270)
		goto L_6270;
	else if (ret == RG_L_6370)
		goto L_6370;

L_6220:
	cout << "IT IS STARDATE " << T << "\n"; goto L_6270;
L_6240:
	cout << "\n"; cout << "THE ENTERPRISE HAS BEEN DESTROYED.  THEN FEDERATION ";
	cout << "WILL BE CONQUERED\n"; goto L_6220;
L_6270:
	cout << "THERE WERE " << K9 << " KLINGON BATTLE CRUISERS LEFT AT\n";
	cout << "THE END OF YOUR MISSION.\n";
L_6290:
	cout << "\n\n"; if (B9 == 0) goto L_6360;
	cout << "THE FEDERATION IS IN NEED OF A NEW STARSHIP COMMANDER\n";
	cout << "FOR A SIMILAR MISSION -- IF THERE IS A VOLUNTEER,\n";
	cout << "LET HIM STEP FORWARD AND ENTER 'AYE'"; cout << "? "; cin >> s_A; if (s_A == "AYE") return;
L_6360:
	exit(0);
L_6370:
	cout << "CONGRULATION, CAPTAIN!  THEN LAST KLINGON BATTLE CRUISER\n";
	cout << "MENACING THE FDERATION HAS BEEN DESTROYED.\n\n";
	cout << "YOUR EFFICIENCY RATING IS " << 1000 * (K7 / (T - T0)) * (K7 / (T - T0)) << "\n"; goto L_6290;
}

// SHORT RANGE SENSOR SCAN & STARTUP SUBROUTINE
void GOSUB_L_6430()
{
	for (I = S1 - 1; I <= S1 + 1; I++) {
		for (J = S2 - 1; J <= S2 + 1; J++) {
			if ((int)(I + 0.5) < 1 || (int)(I + 0.5) > 8 || (int)(J + 0.5) < 1 || (int)(J + 0.5) > 8) goto L_6540;
			s_A = ">!<"; Z1 = I; Z2 = J; GOSUB_L_8830(); if (Z3 == 1) goto L_6580;
L_6540:
			;
		}
	}
	D0 = 0; goto L_6650;
L_6580:
	D0 = 1; s_C = "DOCKED"; E = E0; P = P0;
	cout << "SHIELDS DROPPED FOR DOCKING PURPOSES\n"; S = 0; goto L_6720;
L_6650:
	if (K3 > 0) {
		s_C = "*RED*"; goto L_6720;
	}
	s_C = "GREEN"; if (E < E0 * 0.1) s_C = "YELLOW";
L_6720:
	if (D[2] >= 0) goto L_6770;
	cout << "\n"; cout << "*** SHORT RANGE SENSORS ARE OUT ***\n\n"; return;
L_6770:
	s_O1 = "---------------------------------"; cout << s_O1 << "\n";
	for (I = 1; I <= 8; I++) {
		for (J = (I - 1) * 24 + 1; J <= (I - 1) * 24 + 22; J += 3) {
			cout << " " << b_MID(s_Q, J, 3);
		}
		if (I == 1) goto L_6850;
		if (I == 2) goto L_6900;
		if (I == 3) goto L_6960;
		if (I == 4) goto L_7020;
		if (I == 5) goto L_7070;
		if (I == 6) goto L_7120;
		if (I == 7) goto L_7180;
		if (I == 8) goto L_7240;
L_6850:
		cout << "        STARDATE           " << (int)(T * 10) * 0.1 << "\n"; goto L_7260;
L_6900:
		cout << "        CONDITION          " << s_C << "\n"; goto L_7260;
L_6960:
		cout << "        QUADRANT           " << Q1 << " , " << Q2 << "\n"; goto L_7260;
L_7020:
		cout << "        SECTOR             " << S1 << " , " << S2 << "\n"; goto L_7260;
L_7070:
		cout << "        PHOTON TORPEDOES   " << (int)(P) << "\n"; goto L_7260;
L_7120:
		cout << "        TOTAL ENERGY       " << (int)(E + S) << "\n"; goto L_7260;
L_7180:
		cout << "        SHIELDS            " << (int)(S) << "\n"; goto L_7260;
L_7240:
		cout << "        KLINGONS REMAINING " << (int)(K9) << "\n";
L_7260:
		;
	}
	cout << s_O1 << "\n"; return;
}

// LIBRARY COMPUTER CODE
rg_t GOTO_L_7290()
{
	if (D[8] < 0) {
		cout << "COMPUTER DISABLED\n"; return RG_L_1990;
	}
L_7320:
	cout << "COMPUTER ACTIVE AND AWAITING COMMAND"; cout << "? "; cin >> A; if (A < 0) return RG_L_1990;
	cout << "\n"; H8 = 1; if (A + 1 == 1) goto L_7540;
	if (A + 1 == 2) goto L_7900;
	if (A + 1 == 3) goto L_8070;
	if (A + 1 == 4) goto L_8500;
	if (A + 1 == 5) goto L_8150;
	if (A + 1 == 6) goto L_7400;
	cout << "FUNCTIONS AVAILABLE FROM LIBRARY-COMPUTER:\n";
	cout << "   0 = CUMULATIVE GALACTIC RECORD\n";
	cout << "   1 = STATUS REPORT\n";
	cout << "   2 = PHOTON TORPEDO DATA\n";
	cout << "   3 = STARBASE NAV DATA\n";
	cout << "   4 = DIRECTION/DISTANCE CALCULATOR\n";
	cout << "   5 = GALAXY 'REGION NAME' MAP\n\n"; goto L_7320;
// SETUP TO CHANGE CUM GAL RECORD TO GALAXY MAP
L_7400:
	H8 = 0; G5 = 1; cout << "                        THE GALAXY\n"; goto L_7550;
// CUM GALACTIC RECORD
L_7540:
	// INPUT"DO YOU WANT A HARDCOPY? IS THE TTY ON (Y/N)";A$
// IFA$="Y"THENPOKE1229,2:POKE1237,3:NULL1
	cout << "\n"; cout << "        ";
	cout << "COMPUTER RECORD OF GALAXY FOR QUADRANT " << Q1 << " , " << Q2 << "\n";
	cout << "\n";
L_7550:
	cout << "       1     2     3     4     5     6     7     8\n";
	s_O1 = "     ----- ----- ----- ----- ----- ----- ----- -----";
	cout << s_O1 << "\n";
	for (I = 1; I <= 8; I++) {
		cout << " " << I << " "; if (H8 == 0) goto L_7740;
		for (J = 1; J <= 8; J++) {
			cout << "   "; if (Z[I][J] == 0) {
				cout << "***"; goto L_7720;
			}
			cout << b_RIGHT(b_STR(Z[I][J] + 1000), 3);
L_7720:
			;
		}
		goto L_7850;
L_7740:
		Z4 = I; Z5 = 1; GOSUB_L_9030(); J0 = (int)(15 - 0.5 * b_LEN(s_G2)); cout << b_TAB(J0) << s_G2;
		Z5 = 5; GOSUB_L_9030(); J0 = (int)(39 - 0.5 * b_LEN(s_G2)); cout << b_TAB(J0) << s_G2;
L_7850:
		cout << "\n"; cout << s_O1 << "\n";
	}
	cout << "\n"; return RG_L_1990;
// STATUS REPORT
L_7900:
	cout << "   STATUS REPORT:\n"; s_X = ""; if (K9 > 1) s_X = "S";
	cout << "KLINGON" << s_X << " LEFT:  " << K9 << "\n";
	cout << "MISSION MUST BE COMPLETED IN " << 0.1 * (int)((T0 + T9 - T) * 10) << " STARDATES\n";
	s_X = "S"; if (B9 < 2) {
		s_X = ""; if (B9 < 1) goto L_8010;
	}
	cout << "THE FEDERATION IS MAINTAINING " << B9 << " STARBASE" << s_X << " IN THE GALAXY\n";
	return GOTO_L_5690();
L_8010:
	cout << "YOUR STUPIDITY HAS LEFT YOU ON YOUR ON IN\n";
	cout << "  THE GALAXY -- YOU HAVE NO STARBASES LEFT!\n"; return GOTO_L_5690();
// TORPEDO, BASE NAV, D/D CALCULATOR
L_8070:
	if (K3 <= 0) return GOTO_L_4270();
	s_X = ""; if (K3 > 1) s_X = "S";
	cout << "FROM ENTERPRISE TO KLINGON BATTLE CRUSER" << s_X << "\n";
	H8 = 0; 
	for (I = 1; I <= 3; I++) {
		if (K[I][3] <= 0) goto L_8480;
		W1 = K[I][1]; X = K[I][2];
L_8120:
		C1 = S1; A = S2; goto L_8220;
L_8150:
		cout << "DIRECTION/DISTANCE CALCULATOR:\n";
		cout << "YOU ARE AT QUADRANT  " << Q1 << " , " << Q2 << "  SECTOR  " << S1 << " , " << S2 << "\n";
		cout << "PLEASE ENTER\n"; cout << "  INITIAL COORDINATES (X,Y)"; cout << "? "; cin >> C1; cout << "? "; cin >> A;
		cout << "  FINAL COORDINATES (X,Y)"; cout << "? "; cin >> W1; cout << "? "; cin >> X;
L_8220:
		X = X - A; A = C1 - W1; if (X < 0) goto L_8350;
		if (A < 0) goto L_8410;
		if (X > 0) goto L_8280;
		if (A == 0) {
			C1 = 5; goto L_8290;
		}
L_8280:
		C1 = 1;
L_8290:
		if (abs(A) <= abs(X)) goto L_8330;
		cout << "DIRECTION = " << C1 + (((abs(A) - abs(X)) + abs(A)) / abs(A)) << "\n"; goto L_8460;
L_8330:
		cout << "DIRECTION = " << C1 + (abs(A) / abs(X)) << "\n"; goto L_8460;
L_8350:
		if (A > 0) {
			C1 = 3; goto L_8420;
		}
		if (X != 0) {
			C1 = 5; goto L_8290;
		}
L_8410:
		C1 = 7;
L_8420:
		if (abs(A) >= abs(X)) goto L_8450;
		cout << "DIRECTION = " << C1 + (((abs(X) - abs(A)) + abs(X)) / abs(X)) << "\n"; goto L_8460;
L_8450:
		cout << "DIRECTION = " << C1 + (abs(X) / abs(A)) << "\n";
L_8460:
		cout << "DISTANCE = " << sqrt(X * X + A * A) << "\n"; if (H8 == 1) return RG_L_1990;
L_8480:
		;
	}
	return RG_L_1990;
L_8500:
	if (B3 != 0) {
		cout << "FROM ENTERPRISE TO STARBASE:\n"; W1 = B4; X = B5; goto L_8120;
	}
	cout << "MR. SPOCK REPORTS,  'SENSORS SHOW NO STARBASES IN THIS";
	cout << " QUADRANT.'\n"; return RG_L_1990;
}


// FIND EMPTY PLACE IN QUADRANT (FOR THINGS)
void GOSUB_L_8590()
{
L_8590:
	R1 = b_FNR(1); R2 = b_FNR(1); s_A = "   "; Z1 = R1; Z2 = R2; GOSUB_L_8830(); if (Z3 == 0) goto L_8590;
	return;
}


// INSERT IN STRING ARRAY FOR QUADRANT
void GOSUB_L_8670()
{
	S8 = (int)(Z2 - 0.5) * 3 + (int)(Z1 - 0.5) * 24 + 1;
	if (b_LEN(s_A) != 3) {
		cout << "ERROR\n"; exit(0);
	}
	if (S8 == 1) {
		s_Q = s_A + b_RIGHT(s_Q, 189); return;
	}
	if (S8 == 190) {
		s_Q = b_LEFT(s_Q, 189) + s_A; return;
	}
	s_Q = b_LEFT(s_Q, S8 - 1) + s_A + b_RIGHT(s_Q, 190 - S8); return;
}


// PRINTS DEVICE NAME
void GOSUB_L_8790()
{
	if (R1 == 1) goto L_8792;
	if (R1 == 2) goto L_8794;
	if (R1 == 3) goto L_8796;
	if (R1 == 4) goto L_8798;
	if (R1 == 5) goto L_8800;
	if (R1 == 6) goto L_8802;
	if (R1 == 7) goto L_8804;
	if (R1 == 8) goto L_8806;
	;
L_8792:
	s_G2 = "WARP ENGINES"; return;
L_8794:
	s_G2 = "SHORT RANGE SENSORS"; return;
L_8796:
	s_G2 = "LONG RANGE SENSORS"; return;
L_8798:
	s_G2 = "PHASER CONTROL"; return;
L_8800:
	s_G2 = "PHOTON TUBES"; return;
L_8802:
	s_G2 = "DAMAGE CONTROL"; return;
L_8804:
	s_G2 = "SHIELD CONTROL"; return;
L_8806:
	s_G2 = "LIBRARY-COMPUTER"; return;
}


// STRING COMPARISON IN QUADRANT ARRAY
void GOSUB_L_8830()
{
	Z1 = (int)(Z1 + 0.5); Z2 = (int)(Z2 + 0.5); S8 = (Z2 - 1) * 3 + (Z1 - 1) * 24 + 1; Z3 = 0;
	if (b_MID(s_Q, S8, 3) != s_A) return;
	Z3 = 1; return;
}


// QUADRANT NAME IN G2$ FROM Z4,Z5 (=Q1,Q2)
// CALL WITH G5=1 TO GET REGION NAME ONLY
void GOSUB_L_9030()
{
	if (Z5 <= 4) {
		if (Z4 == 1) goto L_9040;
		if (Z4 == 2) goto L_9050;
		if (Z4 == 3) goto L_9060;
		if (Z4 == 4) goto L_9070;
		if (Z4 == 5) goto L_9080;
		if (Z4 == 6) goto L_9090;
		if (Z4 == 7) goto L_9100;
		if (Z4 == 8) goto L_9110;
		;
	}
	goto L_9120;
L_9040:
	s_G2 = "ANTARES"; goto L_9210;
L_9050:
	s_G2 = "RIGEL"; goto L_9210;
L_9060:
	s_G2 = "PROCYON"; goto L_9210;
L_9070:
	s_G2 = "VEGA"; goto L_9210;
L_9080:
	s_G2 = "CANOPUS"; goto L_9210;
L_9090:
	s_G2 = "ALTAIR"; goto L_9210;
L_9100:
	s_G2 = "SAGITTARIUS"; goto L_9210;
L_9110:
	s_G2 = "POLLUX"; goto L_9210;
L_9120:
	if (Z4 == 1) goto L_9130;
	if (Z4 == 2) goto L_9140;
	if (Z4 == 3) goto L_9150;
	if (Z4 == 4) goto L_9160;
	if (Z4 == 5) goto L_9170;
	if (Z4 == 6) goto L_9180;
	if (Z4 == 7) goto L_9190;
	if (Z4 == 8) goto L_9200;
	;
L_9130:
	s_G2 = "SIRIUS"; goto L_9210;
L_9140:
	s_G2 = "DENEB"; goto L_9210;
L_9150:
	s_G2 = "CAPELLA"; goto L_9210;
L_9160:
	s_G2 = "BETELGEUSE"; goto L_9210;
L_9170:
	s_G2 = "ALDEBARAN"; goto L_9210;
L_9180:
	s_G2 = "REGULUS"; goto L_9210;
L_9190:
	s_G2 = "ARCTURUS"; goto L_9210;
L_9200:
	s_G2 = "SPICA";
L_9210:
	if (G5 != 1) {
		if (Z5 == 1) goto L_9230;
		if (Z5 == 2) goto L_9240;
		if (Z5 == 3) goto L_9250;
		if (Z5 == 4) goto L_9260;
		if (Z5 == 5) goto L_9230;
		if (Z5 == 6) goto L_9240;
		if (Z5 == 7) goto L_9250;
		if (Z5 == 8) goto L_9260;
	}
	return;
L_9230:
	s_G2 = s_G2 + " I"; return;
L_9240:
	s_G2 = s_G2 + " II"; return;
L_9250:
	s_G2 = s_G2 + " III"; return;
L_9260:
	s_G2 = s_G2 + " IV"; return;
}

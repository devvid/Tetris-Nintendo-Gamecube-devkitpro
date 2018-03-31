/*---------------------------------------------------------------------------------

	Tetris by David Cedar Feb 2018 for NGC DevKitPro.   https://www.cedar.net.au
	
	Initial gc code adopted from: nehe lesson 2 port to GX by WinterMute
	And tetris logic adopted from c++ consolegameengine example by Javidx9
	
	Docs & api: http://libogc.devkitpro.org/api_doc.html

---------------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <gccore.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h> // UNIX style

#include <aesndlib.h> // Audio
#include <gcmodplay.h>
// include generated header
#include "tetris_mod.h"

 
#define DEFAULT_FIFO_SIZE	(256*1024)
 
static void *frameBuffer[2] = { NULL, NULL};
GXRModeObj *screenMode;
static MODPlay play;

void drawQuad( float x, float y, float width, float height, int tri, Mtx view);
bool DoesPieceFit(int nTetromino, int nRotation, int nPosX, int nPosY);
int Rotate(int px, int py, int r);
// Manage vector like effects
void push_to_array(int line, int vLines[]);
int get_array_end(int vLines[]);
bool check_array_empty(int vLines[]);
void clear_array(int vLines[]);

// Game Logic variables
float position[2] = {0.0f, 0.0f};
int nFieldWidth = 12;
int nFieldHeight = 18;
unsigned char pField[216]; // nFieldWidth * nFieldHeight
char *tetromino[7]; 
// Colour choices:					Cyan				red					green				blue			orange				purple				pink				yellow			silver 				white
float colorChoices[10][3] = {{0.0f,1.0f,1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f,1.0f,0.4f}, {0.5f,0.5f,1.0f}, {1.0f,0.77f,0.0f}, {0.95f,0.0f,1.0f}, {1.0f,0.0f,0.7f}, {0.95f,1.0f,0.0f}, {0.85f,0.85f,0.85f}, {1.0f,1.0f, 1.0f} };

//---------------------------------------------------------------------------------
int main( int argc, char **argv ){
//---------------------------------------------------------------------------------
	f32 yscale;

	u32 xfbHeight;

	Mtx view;
	Mtx44 perspective;

	u32	fb = 0; 	// initial framebuffer index
	GXColor background = {0, 0, 0, 0xff};


	// init the vi.
	VIDEO_Init();
 
	screenMode = VIDEO_GetPreferredMode(NULL);
	PAD_Init();
	
	// Initialise the audio subsystem
	AESND_Init(NULL);
	
	// allocate 2 framebuffers for double buffering
	frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(screenMode));
	frameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(screenMode));

	VIDEO_Configure(screenMode);
	VIDEO_SetNextFramebuffer(frameBuffer[fb]);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(screenMode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	// setup the fifo and then init the flipper
	void *gp_fifo = NULL;
	gp_fifo = memalign(32,DEFAULT_FIFO_SIZE);
	memset(gp_fifo,0,DEFAULT_FIFO_SIZE);
 
	GX_Init(gp_fifo,DEFAULT_FIFO_SIZE);
 
	// clears the bg to color and clears the z buffer
	GX_SetCopyClear(background, 0x00ffffff);
 
	// other gx setup
	GX_SetViewport(0,0,screenMode->fbWidth,screenMode->efbHeight,0,1);
	yscale = GX_GetYScaleFactor(screenMode->efbHeight,screenMode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0,0,screenMode->fbWidth,screenMode->efbHeight);
	GX_SetDispCopySrc(0,0,screenMode->fbWidth,screenMode->efbHeight);
	GX_SetDispCopyDst(screenMode->fbWidth,xfbHeight);
	GX_SetCopyFilter(screenMode->aa,screenMode->sample_pattern,GX_TRUE,screenMode->vfilter);
	GX_SetFieldMode(screenMode->field_rendering,((screenMode->viHeight==2*screenMode->xfbHeight)?GX_ENABLE:GX_DISABLE));
 
	GX_SetCullMode(GX_CULL_NONE);
	GX_CopyDisp(frameBuffer[fb],GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);
 

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	
	// setup the vertex attribute table
	// describes the data
	// args: vat location 0-7, type of data, data format, size, scale
	// so for ex. in the first call we are sending position data with
	// 3 values X,Y,Z of size F32. scale sets the number of fractional
	// bits for non float data.
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGB8, 0);
 
	GX_SetNumChans(1);
	GX_SetNumTexGens(0);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);

	// setup our camera at the origin
	// looking down the -z axis with y up
	guVector cam = {0.0F, 0.0F, 0.0F},
			up = {0.0F, 1.0F, 0.0F},
		  look = {0.0F, 0.0F, -1.0F};
	guLookAt(view, &cam, &up, &look);
 

	// setup our projection matrix
	// this creates a perspective matrix with a view angle of 90,
	// and aspect ratio based on the display resolution
    f32 w = screenMode->viWidth;
    f32 h = screenMode->viHeight;
	guPerspective(perspective, 45, (f32)w/h, 0.1F, 300.0F);
	GX_LoadProjectionMtx(perspective, GX_PERSPECTIVE);
	
	
	//Play music
	MODPlay_Init(&play);
	MODPlay_SetMOD(&play,tetris_mod);
	MODPlay_Start(&play);
	
	
	//Game Logic Setup tetromino
	tetromino[0] = "..X...X...X...X.";
	tetromino[1] = "..X..XX...X.....";
	tetromino[2] = ".....XX..XX.....";
	tetromino[3] = "..X..XX..X......";
	tetromino[4] = ".X...XX...X.....";
	tetromino[5] = ".X...X...XX.....";
	tetromino[6] = "..X...X..XX.....";
	
	
	
	//pField = unsigned char[nFieldWidth*nFieldHeight]; // Create play field buffer
	//memset(pField, 0, (nFieldWidth*nFieldHeight) );
	for (int x = 0; x < nFieldWidth; x++) // Board Boundary
		for (int y = 0; y < nFieldHeight; y++)
			pField[y*nFieldWidth + x] = (x == 0 || x == nFieldWidth - 1 || y == nFieldHeight - 1) ? 9 : 0;
	
	srand(time(NULL));
	
	bool bKey[4];
	int nCurrentPiece = rand() & 7;
	int nCurrentRotation = 0;
	int nCurrentX = nFieldWidth / 2;
	int nCurrentY = 0;
	int nSpeed = 20;
	int nSpeedCount = 0;
	bool bForceDown = false;
	bool bRotateHold = true;
	int nPieceCount = 0;
	int nScore = 0;
	//vector<int> vLines;
	int vLines[nFieldHeight];
	bool bGameOver = false;
	bool resetGame = false;
	
	//Initialise vLines Array
	for ( int i = 0; i < nFieldHeight; i++)
		vLines[i] = -1;
	

	while(!bGameOver) {
		
		// ######################
		//      Timing
		// ######################
		//usleep(5000);
		nSpeedCount++;
		bForceDown = (nSpeedCount == nSpeed);
		
		// ######################
		//      Game Inputs
		// ######################
		PAD_ScanPads();
		
		//Reset Game pressing start
		if ( PAD_ButtonsDown(0) & PAD_BUTTON_START){
			resetGame = true;
		}
		
		//Reset Handler
		if ( resetGame ){
			nCurrentY = 0;
			nCurrentX = nFieldWidth / 2;
			nCurrentRotation = 0;
			nSpeed = 20;
			nSpeedCount = 0;
			nScore = 0;
			nPieceCount = 0;
			
			//Reset Field
			for (int x = 0; x < nFieldWidth; x++) // Board Boundary
				for (int y = 0; y < nFieldHeight; y++)
					pField[y*nFieldWidth + x] = (x == 0 || x == nFieldWidth - 1 || y == nFieldHeight - 1) ? 9 : 0;
					
			bGameOver = false;
			resetGame = false;
		}
		
		//TODO: Add latching left and right movement. System slows down too much using a simple sleep tick
		
		// Left and Right movement
		bKey[0] = PAD_ButtonsDown(0) & PAD_BUTTON_LEFT ? 1 : 0;
		bKey[1] = PAD_ButtonsDown(0) & PAD_BUTTON_RIGHT ? 1 : 0;
		bKey[2] = PAD_ButtonsHeld(0) & PAD_BUTTON_A ? 1 : 0;
		bKey[3] = PAD_ButtonsHeld(0) & PAD_BUTTON_B ? 1 : 0;
		
		
		// ######################
		//      Logic
		// ######################
		
		// Handle player movement
		nCurrentX -= (bKey[0] && DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX - 1, nCurrentY)) ? 1 : 0;
		nCurrentX += (bKey[1] && DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX + 1, nCurrentY)) ? 1 : 0;
		
		nCurrentY += (bKey[2] && DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY + 1)) ? 1 : 0;
		
		// Rotate, but latch to stop wild spinning
		if (bKey[3])
		{
			nCurrentRotation += (bRotateHold && DoesPieceFit(nCurrentPiece, nCurrentRotation + 1, nCurrentX, nCurrentY)) ? 1 : 0;
			bRotateHold = false;
		}
		else
			bRotateHold = true;
		
		// Force the piece down the playfield if it's time
		if (bForceDown)
		{
			// Update difficulty every 50 pieces
			nSpeedCount = 0;
			nPieceCount++;
			if (nPieceCount % 50 == 0)
				if (nSpeed >= 10) nSpeed--;

			// Test if piece can be moved down
			if (DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY + 1))
				nCurrentY++; // It can, so do it!
			else
			{
				// It can't! Lock the piece in place
				for (int px = 0; px < 4; px++)
					for (int py = 0; py < 4; py++)
						if (tetromino[nCurrentPiece][Rotate(px, py, nCurrentRotation)] != L'.')
							pField[(nCurrentY + py) * nFieldWidth + (nCurrentX + px)] = nCurrentPiece + 1;

				// Check for lines
				for (int py = 0; py < 4; py++)
					if (nCurrentY + py < nFieldHeight - 1)
					{
						bool bLine = true;
						for (int px = 1; px < nFieldWidth - 1; px++)
							bLine &= (pField[(nCurrentY + py) * nFieldWidth + px]) != 0;

						if (bLine)
						{
							// Remove Line, set to =
							for (int px = 1; px < nFieldWidth - 1; px++)
								pField[(nCurrentY + py) * nFieldWidth + px] = 8;
							push_to_array(nCurrentY + py, vLines);
						}
					}

				nScore += 25;
				if (!check_array_empty(vLines))	nScore += (1 << get_array_end(vLines)) * 100;

				// Pick New Piece
				nCurrentX = nFieldWidth / 2;
				nCurrentY = 0;
				nCurrentRotation = 0;
				nCurrentPiece = rand() % 7;

				// If piece does not fit straight away, game over!
				resetGame = !DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY);
				usleep(100000);
			}
		}
		
		
		// ######################
		//      Drawing
		// ######################
		
		// Draw Field
		for (int x = 0; x < nFieldWidth; x++)
			for (int y = 0; y < nFieldHeight; y++)
					if(pField[y * nFieldWidth + x ] != 0)
						drawQuad( x + 2 , y + 2 , 1.0f, 1.0f, pField[y * nFieldWidth + x ], view); 

		// Draw Current Piece
		for (int px = 0; px < 4; px++)
			for (int py = 0; py < 4; py++)
				if (tetromino[nCurrentPiece][Rotate(px, py, nCurrentRotation)] != L'.')
					drawQuad( (nCurrentX + px + 2), (nCurrentY + py + 2), 1.0f, 1.0f, nCurrentPiece+1, view); 


		// Draw Score
		// swprintf_s(&screen[2 * nScreenWidth + nFieldWidth + 6], 16, L"SCORE: %8d", nScore);

		// Animate Line Completion
		if (!check_array_empty(vLines))
		{
			// Display Frame (cheekily to draw lines)
			//WriteConsoleOutputCharacter(hConsole, screen, nScreenWidth * nScreenHeight, { 0,0 }, &dwBytesWritten);
			//this_thread::sleep_for(400ms); // Delay a bit

			//for (auto &v : vLines)
			for(int i = 0; i < get_array_end(vLines) + 1; i++)
				for (int px = 1; px < nFieldWidth - 1; px++)
				{
					for (int py = vLines[i]; py > 0; py--)
						pField[py * nFieldWidth + px] = pField[(py - 1) * nFieldWidth + px];
					pField[px] = 0;
				}

			clear_array(vLines);
		}
		
		
		
		// Drawing The Quad
		//drawQuad( nCurrentX, nCurrentY, 1.0f, 1.0f); 
		
		// do this stuff after drawing
		GX_DrawDone();
		
		// ######################
		//   Finishing Drawing
		// ######################

		fb ^= 1;		// flip framebuffer
		GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
		GX_SetColorUpdate(GX_TRUE);
		GX_CopyDisp(frameBuffer[fb],GX_TRUE);
		VIDEO_SetNextFramebuffer(frameBuffer[fb]);
		VIDEO_Flush();
		VIDEO_WaitVSync();


	}
	
	
	
	
	return 0;
	
	
}
 
// ######################
//      GC Helpers 
// ######################
//---------------------------------------------------------------------------------
void drawQuad( float x, float y, float width, float height, int tri , Mtx view) {
//---------------------------------------------------------------------------------
	// Shifting the screen cords to addapt the array type top left to bottom right cords 
	// to the gc mid zero zero system.
	x = x - 7;
	y = -y + 10;
	
	Mtx model; // model matrix.
	Mtx modelview; // modelview matrix.
	
	// do this before drawing - Applies main transposition for model used. Single model
	GX_SetViewport(0,0,screenMode->fbWidth,screenMode->efbHeight,0,1);
	guMtxIdentity(model);
	guMtxTransApply(model, model, 0.0f, 0.0f, -30.0f);
	guMtxConcat(view,model,modelview);
	// load the modelview matrix into matrix memory
	GX_LoadPosMtxImm(modelview, GX_PNMTX0);	
	
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);															// Draw A Quad
		GX_Position3f32(x, y, 0.0f);																// Top Left
		GX_Color3f32( colorChoices[tri][0], colorChoices[tri][1], colorChoices[tri][2]);
		GX_Position3f32(x+width, y, 0.0f);															// Top Right
		GX_Color3f32( colorChoices[tri][0], colorChoices[tri][1], colorChoices[tri][2]);
		GX_Position3f32(x+width,y+height, 0.0f);													// Bottom Right
		GX_Color3f32( colorChoices[tri][0], colorChoices[tri][1], colorChoices[tri][2]);
		GX_Position3f32(x,y+height, 0.0f);															// Bottom Left
		GX_Color3f32( colorChoices[tri][0], colorChoices[tri][1], colorChoices[tri][2]);
	GX_End();																					// Done Drawing The Quad 

}



// ######################
//  Game Logic Functions
// ######################

int Rotate(int px, int py, int r)
{
	int pi = 0;
	switch (r % 4)
	{
	case 0: // 0 degrees			// 0  1  2  3
		pi = py * 4 + px;			// 4  5  6  7
		break;						// 8  9 10 11
									//12 13 14 15

	case 1: // 90 degrees			//12  8  4  0
		pi = 12 + py - (px * 4);	//13  9  5  1
		break;						//14 10  6  2
									//15 11  7  3

	case 2: // 180 degrees			//15 14 13 12
		pi = 15 - (py * 4) - px;	//11 10  9  8
		break;						// 7  6  5  4
									// 3  2  1  0

	case 3: // 270 degrees			// 3  7 11 15
		pi = 3 - py + (px * 4);		// 2  6 10 14
		break;						// 1  5  9 13
	}								// 0  4  8 12

	return pi;
}

bool DoesPieceFit(int nTetromino, int nRotation, int nPosX, int nPosY)
{
	// All Field cells >0 are occupied
	for (int px = 0; px < 4; px++)
		for (int py = 0; py < 4; py++)
		{
			// Get index into piece
			int pi = Rotate(px, py, nRotation);

			// Get index into field
			int fi = (nPosY + py) * nFieldWidth + (nPosX + px);

			// Check that test is in bounds. Note out of bounds does
			// not necessarily mean a fail, as the long vertical piece
			// can have cells that lie outside the boundary, so we'll
			// just ignore them
			if (nPosX + px >= 0 && nPosX + px < nFieldWidth)
			{
				if (nPosY + py >= 0 && nPosY + py < nFieldHeight)
				{
					// In Bounds so do collision check
					if (tetromino[nTetromino][pi] != L'.' && pField[fi] != 0)
						return false; // fail on first hit
				}
			}
		}

	return true;
}


// ######################################
// Manage vector like effects
// ######################################
void push_to_array(int line, int vLines[])
{
	//Push
	int end = get_array_end(vLines);
	vLines[end] = line+1;
}
int get_array_end(int vLines[])
{
	for (int i = 0; i < nFieldHeight; i++){
		if (vLines[i] == -1) return i;
	}
	return -1;
}
bool check_array_empty(int vLines[])
{
	if (get_array_end(vLines) == -1) return true;
	return false;
}
void clear_array(int vLines[])
{
	for (unsigned i = 0; i < nFieldHeight; i++)
		vLines[i] = -1;
}
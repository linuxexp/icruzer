#include "iDisplay.h"
#pragma warning( disable : 4430 )
#pragma warning( disable : 4996 )
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#include <windows.h>
#include <gl\gl.h>
#include <gl\glu.h>
#include <fstream>
#include <iostream>
using namespace std;
#include "cv.h"
#include "cvaux.h"
#include "cxcore.h"
#include "highgui.h"
#include "resource.h"
#include <commctrl.h>
#include <sapi.h>
#include <stdio.h>
#include <string.h>
#include <atlbase.h>
#include "sphelper.h"

#pragma comment(lib,"sapi.lib")
#pragma comment(lib,"OpenGl32.lib")
#pragma comment(lib,"Glu32.lib")
#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"cv210.lib")
#pragma comment(lib,"cvaux210.lib")
#pragma comment(lib,"cxcore210.lib")
#pragma comment(lib,"highgui210.lib")
#pragma comment(lib,"inpout32.lib")

HWND hWnd = NULL;
HWND hDisplay = NULL; 
static HGLRC hRC = NULL; 
static HDC hDC = NULL;  
int end= 0;
unsigned int VideoSource = 0;
CvCapture* capture = NULL;
BOOL Allow3D = TRUE;
char * filename = NULL;

//Function prototype declaration for Cruzer driver
short _stdcall Inp32(short PortAddress);
void _stdcall Out32(short PortAddress, short data);

/* Routine	   : Out
   Description : Output status while supporting data sharing
				 over different threads
*/
void Out(HWND hDlg,char * szBuffer)
{
   HWND hEdit = GetDlgItem (hDlg, IDC_STATUS);
   int ndx = GetWindowTextLength (hEdit);
   SetFocus (hEdit);
   SendMessage (hEdit, EM_SETSEL, (WPARAM)ndx, (LPARAM)ndx);
   SendMessage (hEdit, EM_REPLACESEL, 0, (LPARAM) ((LPSTR) szBuffer));
}

/*	STRUCT		: Cruzer
	Description : Implements the Hardware-Software interface, 
				  Digital logic and locomotion of the BOT
*/
struct Cruzer
{
   short port;char * dummy;
   char *binprint( unsigned char x, char *buf )
   {int i;
   for( i=0; i<8; i++ )
   buf[7-i]=(x&(1<<i))?'1':'0';
   buf[8]=0;
   return buf;}
   void Current()
   {char * sta=NULL;sta=(char*)malloc(30);
	sprintf(sta,"bin inst : %s\r\n", binprint(Get(),dummy));
	Out(hWnd, sta);
	free(sta);}
   void Set(char sz){Out32(port, sz);}
   char Get(){return Inp32(port);}
   void Reset(){char h=0;Set(h);}
   void Forward(){char fr = 0x30;Set(fr);}
   void Brake(){char br = 0xAA;Set(br);} // BOT destroys itself
   void H1(){char h = 0x10;Set(h);}
   void H2(){char h = 0x20;Set(h);}
   void Backward(){char h = 0xA;Set(h);}
   void TuretLeft(){char h= 0x10;Set(h);}
   void TuretRight(){char h= 0x20;Set(h);}
   void Init(){
   Out(hWnd,"Loading Cruzer Win32 driver module\r\n");
   dummy = (char*) malloc(10);
   port = 0x378;
   Out(hWnd,"Using port 0x378h\r\n");
   Out(hWnd,"Installing & loading Cruzer driver\r\n");
   Out(hWnd,"Allocating memory\r\n");
   Current();Reset();}
   void deinit(){         
   free(dummy);
   Out(hWnd,"Cruzer driver unintialized\r\n");}
};

Cruzer cz; // Instance of Cruzer's Driver 

/* Routine	   : EnableOpenGL
   Description : EnablesOpenGL support, Basically modifies Windows
				 device context(Pixel format) to support OpenGL 
				 rendering context	
*/
void EnableOpenGL (HWND hWnd, HDC *hDC, HGLRC *hRC)
{
    PIXELFORMATDESCRIPTOR pfd;
    int iFormat;
    *hDC = GetDC (hWnd);
    ZeroMemory (&pfd, sizeof (pfd));
    pfd.nSize = sizeof (pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | 
      PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;
    iFormat = ChoosePixelFormat (*hDC, &pfd);
    SetPixelFormat (*hDC, iFormat, &pfd);
    *hRC = wglCreateContext( *hDC );
    wglMakeCurrent( *hDC, *hRC );
}

/* Routine	   : BlockForResult
   Description : Blocks for recognition results and returns	
*/

inline HRESULT BlockForResult(ISpRecoContext * pRecoCtxt, ISpRecoResult ** ppResult)
{
    HRESULT hr = S_OK;
	CSpEvent event;
    while (SUCCEEDED(hr) &&
           SUCCEEDED(hr = event.GetFrom(pRecoCtxt)) &&
           hr == S_FALSE){
    hr = pRecoCtxt->WaitForNotifyEvent(INFINITE);}

    *ppResult = event.RecoResult();
    if (*ppResult){
        (*ppResult)->AddRef();}

    return hr;
}

/* Routine	   : InitSR
   Description : Does everything related to Speech recognition
				 and Speech synthesis
*/

DWORD WINAPI InitSR(HWND hDlg)
{
	Out(hDlg, "Intializing Speech Recognition\r\n\0");
	HRESULT hr = E_FAIL;
    if (SUCCEEDED(hr = ::CoInitialize(NULL)))
    {
        {
            CComPtr<ISpRecoContext> cpRecoCtxt;
            CComPtr<ISpRecoGrammar> cpGrammar;
            CComPtr<ISpVoice> cpVoice;
            hr = cpRecoCtxt.CoCreateInstance(CLSID_SpSharedRecoContext);
            if(SUCCEEDED(hr))
            {
                hr = cpRecoCtxt->GetVoice(&cpVoice);
            }
            if (cpRecoCtxt && cpVoice &&
                SUCCEEDED(hr = cpRecoCtxt->SetNotifyWin32Event()) &&
                SUCCEEDED(hr = cpRecoCtxt->SetInterest(SPFEI(SPEI_RECOGNITION), SPFEI(SPEI_RECOGNITION))) &&
                SUCCEEDED(hr = cpRecoCtxt->SetAudioOptions(SPAO_RETAIN_AUDIO, NULL, NULL)) &&
                SUCCEEDED(hr = cpRecoCtxt->CreateGrammar(0, &cpGrammar)) &&
				SUCCEEDED(hr = cpGrammar->LoadCmdFromFile(L"reco.xml", SPLO_STATIC)) &&
                SUCCEEDED(hr = cpGrammar->SetDictationState(SPRS_INACTIVE)) &&
				SUCCEEDED(hr = cpGrammar->SetRuleIdState(1,SPRS_ACTIVE)))
            {
                USES_CONVERSION;   
                CComPtr<ISpRecoResult> cpResult;
				cpVoice->Speak( L"iCruzer i 3 8 6", SPF_ASYNC, NULL);
                while (SUCCEEDED(hr = BlockForResult(cpRecoCtxt, &cpResult)) && end!=1)
                {
					cpGrammar->SetRuleIdState(1, SPRS_INACTIVE);
                    CSpDynamicString dstrText;
                    if (SUCCEEDED(cpResult->GetText(0, 1, 
                                                    TRUE, &dstrText, NULL)))
                    {
						if(IsDlgButtonChecked(hDlg, IDC_SR)==BST_CHECKED)
						{
						unsigned int vol = SendMessage(GetDlgItem(hDlg, IDC_VOL), TBM_GETPOS, 0, 0);
						unsigned int rate = SendMessage(GetDlgItem(hDlg, IDC_RATE), TBM_GETPOS, 0, 0);
						cpVoice->SetRate(rate);cpVoice->SetVolume(vol);
						SetDlgItemText(hDlg, IDC_RECO, W2A(dstrText));
						if(IsDlgButtonChecked(hDlg, IDC_RESUME)==BST_CHECKED)
						{
							cpVoice->Resume();
						}
						if(IsDlgButtonChecked(hDlg, IDC_PAUSE)==BST_CHECKED)
						{
							cpVoice->Pause();
						}
                        if(_wcsicmp(dstrText, L"yes") ==0){
							cpVoice->Speak( L"Hello ", SPF_DEFAULT, NULL);}	
						if(_wcsicmp(dstrText, L"up") ==0){
							cpVoice->Speak( L"moving forward ", SPF_DEFAULT, NULL);
							SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDC_FORWARD,0),0);}
						if(_wcsicmp(dstrText, L"down") ==0){
							cpVoice->Speak( L"moving backward ", SPF_DEFAULT, NULL);
							SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDC_BACKWARD,0),0);}
						if(_wcsicmp(dstrText, L"right") ==0){
							cpVoice->Speak( L"turning right ", SPF_DEFAULT, NULL);
							SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDC_RIGHT,0),0);}
						if(_wcsicmp(dstrText, L"left") ==0){
							cpVoice->Speak( L"turning left ", SPF_DEFAULT, NULL);
							SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDC_LEFT,0),0);}
						if(_wcsicmp(dstrText, L"hello") ==0){
							cpVoice->Speak( L"I am fine, how you doing ?", SPF_ASYNC, NULL);}
						if(_wcsicmp(dstrText, L"about") ==0){
							SendMessage(hDlg,WM_COMMAND,MAKEWPARAM(IDM_ABOUT1,0),0);
							cpVoice->Speak( L"iCruzer i 3 8 6, is a robotics application developed by Raja. i Display, is the hardware-software interfacer program for the project.", SPF_DEFAULT, NULL);}
						if(_wcsicmp(dstrText, L"help") ==0){
							cpVoice->Speak( L"Opening help", SPF_DEFAULT, NULL);
							SendMessage(hDlg,WM_COMMAND,MAKEWPARAM(IDM_HELP1,0),0);}
						}
                       cpResult.Release();
                    }
					cpGrammar->SetRuleIdState(1, SPRS_ACTIVE);
                } 
            }
        }
        CoUninitialize();
    }
    return hr;
}

/* Routine	   : InitGL
   Description : AKA Core module, does most of the things
				 Basically everything related to visual features
				 and rendering
*/
DWORD WINAPI InitGL(HWND hDlg)
{
	Out(hDlg, "--Core module started\r\n\0");

	BOOL Record = FALSE;
	if(strlen(filename)>1)
		Record = TRUE;

	// Pre-Face detection Initialization
	static CvMemStorage* storage = 0;
	static CvHaarClassifierCascade* cascade = 0;
	const char* cascade_name =
    "face.xml";
	cascade = (CvHaarClassifierCascade*)cvLoad( cascade_name, 0, 0, 0 );
	if( !cascade )
    {Out(hDlg,"ERROR: Could not load classifier cascade\r\n\0" );
	return -1;}
	storage = cvCreateMemStorage(0);
	
	//OpenGL-Initialization and Setup
	// Enable
	hDisplay=GetDlgItem(hDlg,IDC_DISPLAY);
	unsigned int width, height;RECT rect;
	GetWindowRect(hDisplay, &rect);
	width = rect.right;height = rect.bottom;
	EnableOpenGL(hDisplay, &hDC, &hRC);

	//Random value
	float theta = 0.0f;
	
	// Setup projection matrix
	glEnable(GL_DEPTH_TEST);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0f, (float) width / (float) height, 0.1f, 100.1f);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	// Setup Texture
	glEnable(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); 	
	
	// Let there be lights
	glEnable(GL_LIGHTING);

	// Ambient
	GLfloat global_ambient[] = { 0.5f, 0.5f, 0.5f, 1.0f };
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_ambient);
	glShadeModel(GL_SMOOTH);

	// Specular
	GLfloat specular[] = {1.0f, 1.0f, 1.0f , 1.0f};
	glLightfv(GL_LIGHT0, GL_SPECULAR, specular);

	// Set Position of lamp 0
	GLfloat position[] = { 0.0f, -1.0f, 10.0f, 1.0f };
	glLightfv(GL_LIGHT0, GL_POSITION, position);
	glEnable(GL_LIGHT0);
	
	// Tell how to detect face 
	glFrontFace(GL_CCW);

	// Setup Material properties 
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
	glMateriali(GL_FRONT, GL_SHININESS, 96);
	
	// OpenGL + image processing
	IplImage * image = NULL;
	IplImage * logo = cvLoadImage("cruzer.info");
	cvCvtColor(logo, logo, CV_BGR2RGB);
	cvFlip(logo, logo, 0);
	
	double scale = 1.3; // scale images for face detection
	image = cvQueryFrame(capture); // decode frame from camera
	
	// Temp images for face detection
	IplImage* gray = cvCreateImage( cvSize(image->width,image->height), 8, 1 );
    IplImage* small_img = cvCreateImage( cvSize( cvRound (image->width/scale),
                         cvRound (image->height/scale)),
                     8, 1 );
	
		int iheight     = image->height;
		int iwidth      = image->width;
		int step        = image->widthStep/sizeof(unsigned char);
		int channels    = image->nChannels;
	
	CvFont font;
	cvInitFont(&font,CV_FONT_HERSHEY_PLAIN, 2, 0.8, 0, 1);

	// cleanup memory
	*(image->imageData) = NULL;
	int PrevX=0; int PrevY=0; // Holds previous cursor co-ordinates
	

	// BIG WHILE LOOP, everything processing in background is a cause
	// of this

	unsigned int ticks = GetTickCount();unsigned int newtick = GetTickCount();
	//newtick = GetTickCount();
	
	CvVideoWriter * PaVideo = NULL;
	
	if(Record == TRUE){
	PaVideo = cvCreateVideoWriter(filename,CV_FOURCC('x','v','i','d'),10,cvSize(image->width,image->height));
	}

	float DetSize = SendMessage(GetDlgItem(hDlg, IDC_THRES), TBM_GETPOS, 0, 0)/1000;
	
	int wat =0;
	while(end!=1) // check for end of application
	{
		
		//wat = GetTickCount();

		image = cvQueryFrame(capture); // uncompress image 

		if(!image)
			continue;
		
		//cout << "Total processing time " <<  (newtick-ticks) << " ms" <<endl;

		if((newtick - ticks)>100){
		newtick = GetTickCount();ticks = GetTickCount();continue;
		}
		
		//cout << "Query time " << (GetTickCount()-wat) << " ms" << endl;
		
		unsigned char * data    = (unsigned char *)image->imageData;
		
		int ix=0; int iy=0; int thres = SendMessage(GetDlgItem(hDlg, IDC_THRES), TBM_GETPOS, 0, 0);
		int xLaser = 0; int yLaser=0;int countl=0;
		
		//wat = GetTickCount();
		
		int LaserDetOn = IsDlgButtonChecked(hDlg, IDC_LASER);
		int ColorDetOn = IsDlgButtonChecked(hDlg, IDC_CHECKBOX1);
		int incre = SendMessage(GetDlgItem(hWnd, IDC_SLIDER1), TBM_GETPOS, 0, 0);
		
		if(LaserDetOn == BST_UNCHECKED){
		DetSize = (SendMessage(GetDlgItem(hDlg, IDC_THRES), TBM_GETPOS, 0, 0))/100;
		}

		int GBDetOn = IsDlgButtonChecked(hDlg, IDC_CHECKBOX3);

		for(int i=0;i<(iheight);i++)
		{

			for(int j=0;j<(iwidth);j++)
			{
	
				int R = data[i*step+j*channels+2];
				int G = data[i*step+j*channels+1];
				int B = data[i*step+j*channels+0];
				
				
				if( LaserDetOn==BST_CHECKED)
				{
					if(R>thres)
					{
						countl++;
						xLaser = xLaser+j;
						yLaser = yLaser+i;
					}
					if (countl > 0)
					{
					ix = xLaser / countl;
					iy = yLaser / countl;
					}
			
				}  // laser pointer detection

				
				if(ColorDetOn == BST_CHECKED)
				{data[i*step+j*channels+0]+=incre;
				data[i*step+j*channels+1]+=incre;
				data[i*step+j*channels+2]+=incre;
				R+=incre;G+=incre;B+=incre;} // color ++
				
				if(end==1)
					break;
				
				if(GBDetOn==BST_CHECKED)
				{int av = ((0.30*R)+(0.59*G)+(0.11*B));
				data[i*step+j*channels+0]=av;
				data[i*step+j*channels+1]=av;
				data[i*step+j*channels+2]=av;}
				
				

			} // y-axis
			

			if(end==1)
					break;
		} //x-axis
		


		//cout << "Enumertating pixel time " << (GetTickCount()-wat) << " ms" <<endl;

		if(IsDlgButtonChecked(hDlg, IDC_CHECKBOX5)==BST_CHECKED){cvFlip(image, image, 1);}

		if(IsDlgButtonChecked(hDlg, IDC_CHECKBOX4)==BST_CHECKED){
		int pos = SendMessage(GetDlgItem(hDlg, IDC_SLIDER2), TBM_GETPOS, 0, 0);
		cvSmooth(image,image,CV_BLUR,pos,pos);}

		// FACE Detect and rendering
		
		int multi = CV_HAAR_DO_ROUGH_SEARCH;
		
		if(IsDlgButtonChecked(hDlg, IDC_3D)==BST_CHECKED){
		CheckDlgButton(hDlg, IDC_CHECKBOX5, BST_CHECKED);
		multi = CV_HAAR_DO_ROUGH_SEARCH | CV_HAAR_FIND_BIGGEST_OBJECT;
		}

		static CvScalar colors[] = 
		{
        {{0,0,255}},
        {{0,128,255}},
        {{0,255,255}},
        {{0,255,0}},
        {{255,128,0}},
        {{255,255,0}},
        {{255,0,0}},
        {{255,0,255}}
		};

		int i;
		cvCvtColor( image, gray, CV_BGR2GRAY );
		cvResize( gray, small_img, CV_INTER_LINEAR );
		cvEqualizeHist( small_img, small_img );
		cvClearMemStorage( storage );

		if( cascade )
		{CvSeq* faces = cvHaarDetectObjects( small_img, cascade, storage,
                                            1.1, 2,  multi,
                                            cvSize(80, 80) );
		for( i = 0; i < (faces ? faces->total : 0); i++ )
        {
            CvRect* r = (CvRect*)cvGetSeqElem( faces, i );
            CvPoint center;int radius;
            center.x = cvRound((r->x + r->width*0.5)*scale);
            center.y = cvRound((r->y + r->height*0.5)*scale);
            radius = cvRound((r->width + r->height)*0.25*scale);
            cvRectangle( image, cvPoint(r->x*scale,r->y*scale), cvPoint( (r->x+r->width)*scale,(r->y+r->height)*scale),colors[i%8], 1, 8, 0 );
			
			//wat = GetTickCount();

			SendMessage(GetDlgItem(hDlg, IDC_Y), TBM_SETPOS, (WPARAM) TRUE, (LPARAM) 40-((center.x*100)/iwidth));
			SendMessage(GetDlgItem(hDlg, IDC_X), TBM_SETPOS, (WPARAM) TRUE, (LPARAM) 40-((center.y*100)/iheight));
			
			//cout << "Win32 time " << (GetTickCount()-wat) << " ms" << endl;
			
		}
    }
	*(gray->imageData) = NULL;
	*(small_img->imageData) = NULL;

		// FACE END
		unsigned int percentx = (ix*100)/iwidth;
		unsigned int percenty = (iy*100)/iheight;
		// finalizing rendering
		cvLine(image, cvPoint(ix-10,iy), cvPoint(ix+10,iy), cvScalar(0,0,255), 1);
		cvLine(image, cvPoint(ix,iy-10), cvPoint(ix, iy+10), cvScalar(0,0,255), 1);
		// Click point
		cvLine(image, cvPoint((iwidth/2)-5,(iheight/2)), cvPoint((iwidth/2)+5,(iheight/2)), cvScalar(0,255,255), 1);
		cvLine(image, cvPoint((iwidth/2),(iheight/2)-5), cvPoint((iwidth/2), (iheight/2)+5), cvScalar(0,255,255), 1);
		//Put Raja Logo
		cvPutText(image, "Raja's iCruzer\0", cvPoint(0.1*iwidth, 0.1*iheight), &font, cvScalar(0,255,0));

		// Mouse-Laser Interface
		if(IsDlgButtonChecked(hDlg, IDC_LASER)==BST_CHECKED && IsDlgButtonChecked(hDlg, IDC_MOUSE)==BST_CHECKED){
		int movex=(percentx==0)?0:percentx-PrevX;
		int movey=(percenty==0)?0:percenty-PrevY;
		if(PrevX==0){movex=0;}
		if(PrevY==0){movey=0;}
		mouse_event(MOUSEEVENTF_MOVE,movex,movey, NULL,NULL);			
		PrevX = percentx;
		PrevY=  percenty;
		if((movex==0 && movey==0) && (percentx>=40 && percentx<=60) && (percenty>=40 && percenty<=60)){
		mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);Out(hDlg,"Click\r\n\0");}
		}
				if(Record == TRUE){
					cvWriteFrame(PaVideo, image);}
		
		newtick = GetTickCount();

		if(Allow3D==FALSE)
		{
		*(data) = NULL;
		cvShowImage("Optimized", image);
		continue;
		}

		cvCvtColor(image, image, CV_BGR2RGB);
		cvFlip(image, image, 0);
		
		if(end==1)
					break;

		/* Animation */
		
            glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
            glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glPushMatrix();
			gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, logo->width, logo->height, GL_RGB, GL_UNSIGNED_BYTE, logo->imageData);
			glBegin(GL_POLYGON);
            glColor3f (1.0f, 1.0f, 1.0f);
			glTexCoord2f(1.0f,1.0f);glVertex3f(4.75f,3.3f,-8.0f); // right top
			glTexCoord2f(0.0f,1.0f);glVertex3f(-4.75f,3.3f,-8.0f); //left top
            glTexCoord2f(0.0f,0.0f);glVertex3f(-4.75f,-3.3f,-8.0f); //left bottom
            glTexCoord2f(1.0f,0.0f);glVertex3f(4.75f,-3.3f,-8.0f); // right bottom
            glEnd ();
			glPopMatrix();
			glLoadIdentity();
			glTranslatef (0.0f,-0.0f, -3.0f); //-3.0f
            
			// Create Texture
			//wat = GetTickCount();

			gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, image->width, image->height, GL_RGB, GL_UNSIGNED_BYTE, image->imageData);
			//cout << "Texture building " << (GetTickCount()-wat) << " ms"<<endl;
			glPushMatrix ();
			glRotatef((float)((IsDlgButtonChecked(hDlg,IDC_RANDOM)==BST_CHECKED) ? theta : SendMessage(GetDlgItem(hDlg, IDC_Y), TBM_GETPOS, 0, 0)), 0.0f, 1.0f, 0.0f); // rotate on y
			glRotatef((float) ((IsDlgButtonChecked(hDlg,IDC_RANDOM)==BST_CHECKED) ? theta : SendMessage(GetDlgItem(hDlg, IDC_X), TBM_GETPOS, 0, 0)), 1.0f, 0.0f, 0.0f); // rotate on x
			glBegin(GL_POLYGON);
			glColor3f (1.0f, 1.0f, 1.0f);
			glTexCoord2f(1.0f, 1.0f);glVertex3f(1.97f,1.23f,0.0f); // right top
			glTexCoord2f(0.0f, 1.0f);glVertex3f(-1.97f,1.23f,0.0f); //left top
            glTexCoord2f(0.0f, 0.0f);glVertex3f(-1.97f,-1.23f,0.0f); //left bottom
            glTexCoord2f(1.0f, 0.0f);glVertex3f(1.97f,-1.23f,0.0f); // right bottom
			glEnd ();
            glPopMatrix ();

            SwapBuffers (hDC); // We are double buffering
			*(data)=NULL;
			theta -= (float)(GetDlgItemInt(hDlg, IDC_SPEED, FALSE, TRUE)==0?1.0f:GetDlgItemInt(hDlg, IDC_SPEED, FALSE, TRUE));
            Sleep (1);	
	}
	
	// Release memory
	cvDestroyAllWindows();
	cvReleaseVideoWriter(&PaVideo);
	cvReleaseImage(&logo);
	cvReleaseImage(&image);
	cvReleaseImage( &gray );
    cvReleaseImage( &small_img );
	wglMakeCurrent(NULL, NULL);
	ReleaseDC (hDlg, hDC) ;
	wglDeleteContext(hRC);
	return TRUE;
}

/* Routine	   : HelpDlgProc
   Description : Dialog procedure for the Help Dialog	
*/
BOOL CALLBACK HelpDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{	char * help = NULL;
	switch(msg)
    {
        case WM_INITDIALOG:
			{
		help =(char*) malloc(2000);
		ifstream fin("help.txt",ios::in);
		HWND hEdit = GetDlgItem (hwnd, IDC_EDIT1);
		while(fin.eof()!=TRUE)
		{
		fin.getline(help,2000);
		sprintf(help,"%s\r\n",help);
		int ndx = GetWindowTextLength (hEdit);
		SetFocus (hEdit);
		SendMessage (hEdit, EM_SETSEL, (WPARAM)ndx, (LPARAM)ndx);
		SendMessage (hEdit, EM_REPLACESEL, 0, (LPARAM) ((LPSTR) help));
		}
		fin.close();
		free(help);
			}return TRUE;
		case WM_CLOSE:
		EndDialog(hwnd, 0);
		return TRUE;
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {case IDOK:
			SendMessageA(hwnd, WM_CLOSE, 0, 0);
			break;
            }break;
        default:
            return FALSE;}
    return TRUE;
}

/* Routine	   : AboutDlgProc
   Description : Dialog procedure for the About Dialog	
*/
BOOL CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
    {
        case WM_INITDIALOG:
	    return TRUE;
		case WM_CLOSE:
		EndDialog(hwnd, 0);
		return TRUE;
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {case IDOK:
			SendMessageA(hwnd, WM_CLOSE, 0, 0);
			break;}
        break;
        default:
        return FALSE;
    }return TRUE;
}

/* Routine	   : VideoDlgProc
   Description : Dialog procedure for the Video Dialog	
*/
BOOL CALLBACK VideoDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
    {
        case WM_INITDIALOG:
	    return TRUE;
		case WM_CLOSE:
		GetDlgItemText(hwnd, IDC_FILENAME, filename, 255);
		EndDialog(hwnd, 0);
		return TRUE;
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {case IDOK:
			 {VideoSource = GetDlgItemInt(hwnd, IDC_SOURCE, NULL, TRUE);
			  SendMessageA(hwnd, WM_CLOSE, 0, 0);}
			  break;
			  case IDCANCEL:
			  EndDialog(hwnd, IDCANCEL);
			  break;
            }
        break;
        default:
        return FALSE;
    }return TRUE;
}


/* Routine	   : AppDlgProc
   Description : Dialog procedure for the Application Dialog	
*/
BOOL CALLBACK AppDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HANDLE hOpenGL=NULL;
	static DWORD lpThreadId=NULL;
	static HANDLE hSR = NULL;
	static DWORD hSRId = NULL;
	HWND SliderX = NULL;
	HWND SliderY = NULL;
    switch(msg)
    {
		case WM_INITDIALOG:
		{
		hWnd = hwnd;cz.Init();
		SliderX = GetDlgItem(hwnd, IDC_X);SliderY = GetDlgItem(hwnd, IDC_Y);
		SendMessage(SliderX, TBM_SETRANGE,(WPARAM) TRUE,(LPARAM) MAKELONG(-40, 40));
		SendMessage(SliderY, TBM_SETRANGE,(WPARAM) TRUE,(LPARAM) MAKELONG(-40, 40));
		SendMessage(GetDlgItem(hwnd, IDC_SLIDER1), TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) MAKELONG(0,255));
		SendMessage(GetDlgItem(hwnd, IDC_SLIDER2), TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) MAKELONG(1,50));
		SendMessage(GetDlgItem(hwnd, IDC_THRES), TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) MAKELONG(1,255));
		SendMessage(GetDlgItem(hwnd, IDC_THRES), TBM_SETPOS, (WPARAM) TRUE, (LPARAM) 230);
		SendMessage(GetDlgItem(hwnd, IDC_VOL), TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) MAKELONG(0,100));
		SendMessage(GetDlgItem(hwnd, IDC_VOL), TBM_SETPOS, (WPARAM) TRUE, (LPARAM) 100);
		SendMessage(GetDlgItem(hwnd, IDC_RATE), TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) MAKELONG(0,20));
		SendMessage(GetDlgItem(hwnd, IDC_RATE), TBM_SETPOS, (WPARAM) TRUE, (LPARAM) 2);
		hOpenGL=CreateThread(NULL,NULL,(LPTHREAD_START_ROUTINE)InitGL,hwnd,0,&lpThreadId);
		hSR=CreateThread(NULL,NULL,(LPTHREAD_START_ROUTINE)InitSR,hwnd,0,&hSRId);
		}
        return TRUE;
		case WM_CLOSE:
			{
		end=1;cz.deinit();Sleep(1000);
		TerminateThread(hOpenGL, 0);
		TerminateThread(hSR, 0);
		cvReleaseCapture(&capture);
		CoUninitialize();
		EndDialog(hwnd, 0);
			}
		return TRUE;
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
			case IDC_FORWARD:
				cz.Forward();cz.Current();Sleep(500);cz.Reset();
			break;
			case IDC_BACKWARD:
			cz.Backward();cz.Current();Sleep(500);cz.Reset();
			break;
			case IDC_RIGHT:
			cz.H1();cz.Current();Sleep(500);cz.Reset();
			break;
			case IDC_LEFT:
			cz.H2();cz.Current();Sleep(500);cz.Reset();
			break;
			case IDC_TUR1:
			cz.TuretLeft();cz.Current();Sleep(500);cz.Reset();
			break;
			case IDC_TUR2:
			cz.TuretRight();cz.Current();Sleep(500);cz.Reset();
			break;
			case IDM_ABOUT1:
			DialogBoxA(GetModuleHandle(NULL), 
            MAKEINTRESOURCEA(IDD_ABOUT), hwnd, AboutDlgProc);
			break;
			case IDM_HELP1:
			DialogBoxA(GetModuleHandle(NULL), 
            MAKEINTRESOURCEA(IDD_HELP), hwnd, HelpDlgProc);
			break;
			case IDC_HELP1:
			SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_HELP1,0),0);
			break;
            case IDOK:
			SendMessageA(hwnd, WM_CLOSE, 0, 0);
			break;
            }
        break;

		case WM_LBUTTONDOWN:
		cz.Forward();cz.Current();
		break;
		case WM_LBUTTONUP:
		cz.Reset();cz.Current();
		break;

		case WM_RBUTTONDOWN:
		cz.Backward();cz.Current();
		break;
		case WM_RBUTTONUP:
		cz.Reset();cz.Current();
		break;

        default:
        return FALSE;
    }return TRUE;
}

/* Routine	   : WinMain
   Description : Application Start	
*/
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
//int main()
{
	filename = (char*) malloc(300);
	CoInitializeEx(NULL,NULL);InitCommonControls();
	int ret = DialogBoxA(GetModuleHandle(NULL),MAKEINTRESOURCEA(IDD_VIDEO), NULL, VideoDlgProc);
	if(ret==IDCANCEL)
	return 1;
	
	capture = cvCaptureFromCAM( VideoSource );
	//capture = cvCaptureFromFile("trees.avi\0");
	if( !capture ) {MessageBox(NULL, "Failed to capture from Video Source!", 0,0);
	return -1;}
	
	if(MessageBox(NULL,"Start Optimized iDisplay(3D FX Disabled)","Optimized Display",MB_YESNO)==IDYES){
	Allow3D = FALSE;cvNamedWindow("Optimized",1);}

	DialogBoxA(GetModuleHandle(NULL),MAKEINTRESOURCEA(IDD_APP), NULL, AppDlgProc);
	free(filename);
	return 0;
}
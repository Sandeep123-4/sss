#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <gdiplus.h>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <sstream>

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"gdiplus.lib")

using namespace Gdiplus;

SOCKET sock;
ULONG_PTR gdiplusToken;

HWND hMainWnd, hInput, hSendBtn;
HWND hLoginWnd, hLoginIP, hLoginUser, hLoginBtn;

std::mutex mtx;

struct Message {
    std::string text;
    bool isMe;
};

std::vector<Message> messages;
std::vector<std::string> users;

std::string loginUser;
std::string loginIP;

// ---------------- Rounded Rectangle ----------------
void FillRoundRect(Graphics &g, Brush* brush, RectF r, float radius) {
    GraphicsPath path;
    path.AddArc(r.X, r.Y, radius*2, radius*2, 180, 90);
    path.AddArc(r.X+r.Width-radius*2, r.Y, radius*2, radius*2, 270, 90);
    path.AddArc(r.X+r.Width-radius*2, r.Y+r.Height-radius*2, radius*2, radius*2, 0, 90);
    path.AddArc(r.X, r.Y+r.Height-radius*2, radius*2, radius*2, 90, 90);
    path.CloseFigure();
    g.FillPath(brush, &path);
}

// ---------------- Send Message ----------------
void sendMessage() {
    char buf[512];
    GetWindowText(hInput, buf, 512);
    if(strlen(buf)==0) return;

    send(sock, buf, strlen(buf), 0);
    SetWindowText(hInput,"");

    std::lock_guard<std::mutex> lock(mtx);
    messages.push_back({buf,true});
    InvalidateRect(hMainWnd,NULL,TRUE);
}

// ---------------- Receive Messages ----------------
void receiveMessages() {
    char buffer[1024];

    while(true) {

        int bytes = recv(sock, buffer, sizeof(buffer)-1, 0);
        if(bytes <= 0) break;

        buffer[bytes] = '\0';
        std::string msg(buffer);

        // -------- USERS LIST --------
        if(msg.rfind("USERS:",0) == 0) {

            std::lock_guard<std::mutex> lock(mtx);
            users.clear();

            std::string list = msg.substr(6);
            std::stringstream ss(list);
            std::string name;

            while(std::getline(ss,name,',')){
                users.push_back(name);
            }

            InvalidateRect(hMainWnd,NULL,TRUE);
            continue;
        }

        // -------- REMOVE JOIN MESSAGE --------
        if(msg.find("joined the chat") != std::string::npos){
            continue;
        }

        // -------- PREVENT DOUBLE MESSAGE --------
        if(msg.find(loginUser + ":") == 0){
            continue;
        }

        std::lock_guard<std::mutex> lock(mtx);
        messages.push_back({msg,false});
        InvalidateRect(hMainWnd,NULL,TRUE);
    }
}

// ---------------- Draw Chat ----------------
void drawChat(HDC hdc, RECT rc){
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    int sidebarWidth = 180;
    int chatWidth = rc.right - sidebarWidth;

    LinearGradientBrush bg(
        Point(0,0),
        Point(0,rc.bottom),
        Color(255,240,242,245),
        Color(255,225,229,235)
    );
    g.FillRectangle(&bg,0,0,rc.right,rc.bottom);

    FontFamily fontFamily(L"Segoe UI");
    Font font(&fontFamily,14,FontStyleRegular,UnitPixel);

    int y = 20;

    std::lock_guard<std::mutex> lock(mtx);

    for(auto &m : messages){

        RectF layoutRect(0,0,260,1000);
        RectF bound;
        StringFormat format;

        std::wstring text(m.text.begin(), m.text.end());
        g.MeasureString(text.c_str(), -1, &font, layoutRect, &bound);

        float bubbleWidth  = bound.Width + 25;
        float bubbleHeight = bound.Height + 18;

        float x = m.isMe ? chatWidth - bubbleWidth - 25 : 25;

        RectF bubbleRect(x,y,bubbleWidth,bubbleHeight);

        SolidBrush bubbleBrush(
            m.isMe ?
            Color(255,0,132,255) :
            Color(255,255,255)
        );

        FillRoundRect(g,&bubbleBrush,bubbleRect,18);

        SolidBrush textBrush(
            m.isMe ?
            Color(255,255,255) :
            Color(255,30,30,30)
        );

        RectF textRect(x+12,y+8,bound.Width+5,bound.Height+5);
        g.DrawString(text.c_str(),-1,&font,textRect,&format,&textBrush);

        y += bubbleHeight + 15;
    }

    // Sidebar
    SolidBrush sideBg(Color(255,35,40,48));

    RectF sideRect(
        (REAL)(rc.right - sidebarWidth),
        0,
        (REAL)sidebarWidth,
        (REAL)rc.bottom
    );

    g.FillRectangle(&sideBg, sideRect);

    Font sideFont(&fontFamily,13,FontStyleRegular,UnitPixel);
    SolidBrush sideText(Color(255,220,220,220));

    int sy = 25;

    for(auto &u : users){

        SolidBrush avatarBrush(Color(255,90,120,255));

        g.FillEllipse(&avatarBrush,
            RectF((REAL)(rc.right - sidebarWidth + 20),(REAL)sy,35,35)
        );

        RectF nameRect(
            (REAL)(rc.right - sidebarWidth + 70),
            (REAL)(sy + 8),
            120,
            30
        );

        std::wstring uname(u.begin(),u.end());
        StringFormat sf;
        g.DrawString(uname.c_str(), -1, &sideFont, nameRect, &sf, &sideText);

        sy += 55;
    }
}

// ---------------- Chat Window ----------------
LRESULT CALLBACK ChatProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp){
    switch(msg){
    case WM_COMMAND:
        if((HWND)lp==hSendBtn) sendMessage();
        break;

    case WM_PAINT:{
        PAINTSTRUCT ps;
        HDC hdc=BeginPaint(hwnd,&ps);
        RECT rc; GetClientRect(hwnd,&rc);
        drawChat(hdc,rc);
        EndPaint(hwnd,&ps);
        break;
    }

    case WM_SIZE:{
        RECT rc; GetClientRect(hwnd,&rc);
        int h = rc.bottom-rc.top;
        MoveWindow(hInput,20,h-50,rc.right-240,35,TRUE);
        MoveWindow(hSendBtn,rc.right-200,h-50,120,35,TRUE);
        break;
    }

    case WM_DESTROY:
        closesocket(sock);
        WSACleanup();
        GdiplusShutdown(gdiplusToken);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd,msg,wp,lp);
}

// ---------------- Login Window ----------------
LRESULT CALLBACK LoginProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp){
    switch(msg){
    case WM_COMMAND:
        if((HWND)lp==hLoginBtn){

            char buf[100];

            GetWindowText(hLoginIP,buf,100);
            loginIP = buf;

            GetWindowText(hLoginUser,buf,100);
            loginUser = buf;

            if(loginIP.empty() || loginUser.empty()){
                MessageBox(hwnd,"Fill all fields","Error",MB_OK);
                return 0;
            }

            sockaddr_in server{};
            server.sin_family=AF_INET;
            server.sin_port=htons(9000);
            server.sin_addr.s_addr=inet_addr(loginIP.c_str());

            if(connect(sock,(sockaddr*)&server,sizeof(server))==SOCKET_ERROR){
                MessageBox(hwnd,"Connection Failed","Error",MB_OK);
                return 0;
            }

            send(sock,loginUser.c_str(),loginUser.size(),0);

            DestroyWindow(hLoginWnd);
            ShowWindow(hMainWnd,SW_SHOW);

            std::thread(receiveMessages).detach();
        }
        break;
    }
    return DefWindowProc(hwnd,msg,wp,lp);
}

// ---------------- WinMain ----------------
int WINAPI WinMain(HINSTANCE hInst,HINSTANCE,LPSTR,int nCmdShow){

    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2),&wsa);
    sock = socket(AF_INET,SOCK_STREAM,0);

    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken,&gdiplusStartupInput,NULL);

    WNDCLASS wc{};
    wc.hInstance=hInst;
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);

    wc.lpfnWndProc=LoginProc;
    wc.lpszClassName="LoginWindow";
    RegisterClass(&wc);

    hLoginWnd=CreateWindow("LoginWindow","Login",
        WS_OVERLAPPEDWINDOW,
        400,200,350,200,
        NULL,NULL,hInst,NULL);

    CreateWindow("STATIC","Server IP:",
        WS_CHILD|WS_VISIBLE,
        20,20,80,20,
        hLoginWnd,NULL,hInst,NULL);

    hLoginIP=CreateWindow("EDIT","127.0.0.1",
        WS_CHILD|WS_VISIBLE|WS_BORDER,
        110,20,180,25,
        hLoginWnd,NULL,hInst,NULL);

    CreateWindow("STATIC","Username:",
        WS_CHILD|WS_VISIBLE,
        20,60,80,20,
        hLoginWnd,NULL,hInst,NULL);

    hLoginUser=CreateWindow("EDIT","",
        WS_CHILD|WS_VISIBLE|WS_BORDER,
        110,60,180,25,
        hLoginWnd,NULL,hInst,NULL);

    hLoginBtn=CreateWindow("BUTTON","Login",
        WS_CHILD|WS_VISIBLE,
        110,100,100,30,
        hLoginWnd,NULL,hInst,NULL);

    ShowWindow(hLoginWnd,nCmdShow);

    wc.lpfnWndProc=ChatProc;
    wc.lpszClassName="ModernMessenger";
    RegisterClass(&wc);

    hMainWnd=CreateWindow("ModernMessenger","Modern Chat",
        WS_OVERLAPPEDWINDOW,
        300,100,750,550,
        NULL,NULL,hInst,NULL);

    hInput=CreateWindow("EDIT","",
        WS_CHILD|WS_VISIBLE|WS_BORDER,
        20,450,400,35,
        hMainWnd,NULL,hInst,NULL);

    hSendBtn=CreateWindow("BUTTON","Send",
        WS_CHILD|WS_VISIBLE,
        430,450,120,35,
        hMainWnd,NULL,hInst,NULL);

    ShowWindow(hMainWnd,SW_HIDE);

    MSG msg;
    while(GetMessage(&msg,NULL,0,0)){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

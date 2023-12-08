#include "ControlPannel.h"
#include <stdio.h>
#include <process.h>

ControlPannel_Window *CP;
ComboboxData CB_Data;
int mFocus = 0;// 
const int num_List = 4;
WCHAR	CombList[num_List][32] = { L"相机查询中",L"RGB_Camera", L"Mono_Camera",L"TopCamera" };
WCHAR	FmtList[num_List][32] = { L"RGB",L"Gray", L"HSV",L"Processed" };
char msg_CP[256] = { 0 };	// 控制面板子窗口的消息字符串

// 显示数据的线程
UINT WINAPI DispThread_CP(void* pParam)
{
	ControlPannel_Window *CP = (ControlPannel_Window*)pParam;
	LARGE_INTEGER	freq, TimeStamp, LastTimeStamp;	// 节拍控制的CPU计数变量
	QueryPerformanceCounter(&LastTimeStamp);

	while (CP->bDispData)// 退出时首先释放本线程
	{
		// 等待线程退出的内核对象
		::WaitForSingleObject(CP->hRefreshCP, INFINITE);

		QueryPerformanceFrequency(&freq);	// 获取CPU主频
		QueryPerformanceCounter(&TimeStamp);

		UINT mElapsed = (UINT)((TimeStamp.QuadPart - LastTimeStamp.QuadPart) * 1000.0 / freq.QuadPart);
		if (mElapsed < 499)
			continue;

		LastTimeStamp = TimeStamp;

		if (CP->bComm)
		{
			sprintf_s(CP->TxInf, "L:%03d Rx:%d Tx:%d", CP->TxTicks - CP->RxTicks, CP->RxTicks, CP->TxTicks);
			SendMessage(CP->hStaticTx, WM_SETTEXT, true, (LPARAM)ConvertChar(CP->TxInf));

			sprintf_s(CP->RxInf, "%d/%d", CP->DataTicks, CP->DealTicks);
			SendMessage(CP->hStaticRx, WM_SETTEXT, true, (LPARAM)ConvertChar(CP->RxInf));

			sprintf_s(CP->DelayInf, "Rx:%d/UI:%d Delay:%03dus", CP->mTimeDelay/1000, CP->mUIDelay/1000, CP->mUIDelay - CP->mTimeDelay);
			SendMessage(CP->hStaticDelay, WM_SETTEXT, true, (LPARAM)ConvertChar(CP->DelayInf));

			SendMessage(CP->hStaticRxData, WM_SETTEXT, true, (LPARAM)ConvertChar(CP->RxData));
		}

		// 相机参数
		sprintf_s(CP->fpsInf, MAX_PATH, "%04.2f/%05.0f us/Frame", CP->Fps, ((double)CP->Process_us + CP->TrigUs));
		SendMessage(CP->hFPSInf, WM_SETTEXT, true, (LPARAM)ConvertChar(CP->fpsInf));

		sprintf_s(CP->algInf, MAX_PATH, "%05.0f Proc: %05.0f/Trig: %05.0f", CP->Algrithm_us, CP->Process_us, CP->TrigUs);
		SendMessage(CP->hAlgInf, WM_SETTEXT, true, (LPARAM)ConvertChar(CP->algInf));

		sprintf_s(CP->focusInf, "%d", CP->FocusValue);
		SendMessage(CP->hFocusInf, WM_SETTEXT, true, (LPARAM)ConvertChar(CP->focusInf));
	}

	SetEvent(CP->hDisplay);	// 置位发送线程标志位
	_endthreadex(0);
	return 0;
}

// 创建控制面板窗口
void CreateControlPannel(HWND hparent, HINSTANCE g_instance, ControlPannel_Window * pWindow,int left,int top,bool bPLC)
{
	ControlPannel_Window* lpCP = pWindow;
	CP = lpCP;
	lpCP->hWndParent = hparent;
	lpCP->hWndPannel = CreateDialog(g_instance, MAKEINTRESOURCE(IDD_ControlPannel), hparent, (DLGPROC)CtrlPannelProc);

	// 手动设置信号状态 ，初始化为无信号状态
	lpCP->hRefreshCP = CreateEvent(NULL, FALSE, FALSE, NULL);

	// 创建发送线程的函数
	lpCP->hDisplay = (HANDLE)_beginthreadex(NULL, 0, DispThread_CP, CP, 0, &CP->mThreadID);

	if (lpCP->hDisplay)
		SetThreadPriority(lpCP->hDisplay, THREAD_PRIORITY_LOWEST);

	RECT mRect; POINT point;
	GetWindowRect(lpCP->hWndPannel, &mRect);
	lpCP->bComm = bPLC;
	if (!bPLC)
		mRect.bottom -= 120;

	point = { left, top };
	ClientToScreen(hparent, &point);	// 客户坐标转换为屏幕坐标
	MoveWindow(lpCP->hWndPannel, point.x, point.y, mRect.right- mRect.left, mRect.bottom- mRect.top, 1);

	lpCP->hCombBox = GetDlgItem(lpCP->hWndPannel, IDC_CB_CAMERA);// 相机列表COMBBOX控件的句柄
	
	for (int i = 0; i < num_List; ++i)
	{
		SendMessage(lpCP->hCombBox, (UINT)CB_INSERTSTRING, (WPARAM)i, (LPARAM)CombList[i]);
		SendMessage(lpCP->hCombBox, (UINT)CB_SETITEMDATA, i, i);
	}
	SendMessage(pWindow->hCombBox, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);

	lpCP->hCombBFmt = GetDlgItem(lpCP->hWndPannel, IDC_CB_Fmt);// 图像格式列表COMBBOX控件的句柄
	for (int i = 0; i < num_List; ++i)
	{
		SendMessage(lpCP->hCombBFmt, (UINT)CB_INSERTSTRING, (WPARAM)i, (LPARAM)FmtList[i]);
		SendMessage(lpCP->hCombBFmt, (UINT)CB_SETITEMDATA, i, i);
	}
	SendMessage(lpCP->hCombBFmt, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);


	sprintf_s(msg_CP, "\r\nControl Pannel Created!\r\n");
	SendMessage(hparent, ContrlPannelMsg, (WPARAM)textMsg, (LPARAM)msg_CP);

	lpCP->hSnaps = GetDlgItem(lpCP->hWndPannel, IDC_EDIT_Snaps);		// 连拍张数文本框的句柄
	SendMessage(lpCP->hSnaps, WM_SETTEXT, true, (LPARAM)L"10");

	lpCP->hResolutin= GetDlgItem(lpCP->hWndPannel, IDC_Resolution);	// 分辨率与曝光时间的句柄
	SendMessage(lpCP->hResolutin, WM_SETTEXT, true, (LPARAM)L"0*0/0us");

	lpCP->hFocusCHK = GetDlgItem(lpCP->hWndPannel, IDC_CHK_Focus);	// 合焦检查框的句柄
	lpCP->hFocusInf = GetDlgItem(lpCP->hWndPannel, IDC_FocusInf);		// 合焦数据的句柄
	SendMessage(lpCP->hFocusInf, WM_SETTEXT, true, (LPARAM)L"0");

	lpCP->hFPSInf = GetDlgItem(lpCP->hWndPannel, IDC_FPSinf);			// 相机帧率的句柄
	lpCP->hAlgInf = GetDlgItem(lpCP->hWndPannel, IDC_ALGinf);			// 相机帧率的句柄

	if (bPLC)
	{
		lpCP->hStaticTx = GetDlgItem(lpCP->hWndPannel, IDC_TxStatus);	// 发送数据的显示窗口句柄
		lpCP->hStaticRx = GetDlgItem(lpCP->hWndPannel, IDC_RxStatus);	// 接收数据的显示窗口句柄
		lpCP->hStaticDelay = GetDlgItem(lpCP->hWndPannel, IDC_WriteDelay);	// 显示延时信息窗口句柄
		lpCP->hStaticRxData = GetDlgItem(lpCP->hWndPannel, IDC_RxData);	// 接收到的数据的显示窗口的句柄
		
		SendMessage(lpCP->hStaticTx, WM_SETTEXT, true, (LPARAM)L"Tx:0");
		SendMessage(lpCP->hStaticRx, WM_SETTEXT, true, (LPARAM)L"Rx:0");
		SendMessage(lpCP->hStaticDelay, WM_SETTEXT, true, (LPARAM)L"Rx:0ms");
		SendMessage(lpCP->hStaticRxData, WM_SETTEXT, true, (LPARAM)L"RxData:0");
	}
}


// char 转 LPCWSTR 
LPCWSTR ConvertChar(char*msg)
{
	char src[4096] = "";
	strcat_s(src, msg);
	int len_in = (int)strlen(src);
	if (len_in == 0)
		return NULL;

	len_in++;
	src[len_in] = 0;
	LPWSTR lpszPath = new WCHAR[len_in];
	MultiByteToWideChar(CP_ACP, 0, src, len_in, lpszPath, len_in);
	return lpszPath;
}

void SetComboBoxItem(ControlPannel_Window * pWindow, ComboboxData * cbd,int num)
{
	SendMessage(pWindow->hCombBox, (UINT)CB_RESETCONTENT, 0, 0);
	
	for (int i = 0; i < num; ++i)
	{
		SendMessage(pWindow->hCombBox, (UINT)CB_INSERTSTRING, (WPARAM)i, (LPARAM)ConvertChar(cbd[i].Str));
		SendMessage(pWindow->hCombBox, (UINT)CB_SETITEMDATA, i, cbd[i].ID);
	}
	SendMessage(pWindow->hCombBox, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
}

/******************************************************************************************
Function:        ConvertLPWSTRToLPSTR
Description:     LPWSTR转char*
Input:           lpwszStrIn:待转化的LPWSTR类型
Return:          转化后的char*类型
*******************************************************************************************/
char* ConvertLPWSTRToLPSTR(LPWSTR lpwszStrIn)
{
	LPSTR pszOut = NULL;
	if (lpwszStrIn != NULL)
	{
		int nInputStrLen = (int)wcslen(lpwszStrIn);

		// Double NULL Termination  
		int nOutputStrLen = WideCharToMultiByte(CP_ACP, 0, lpwszStrIn, nInputStrLen, NULL, 0, 0, 0) + 2;
		pszOut = new char[nOutputStrLen];

		if (pszOut)
		{
			memset(pszOut, 0x00, nOutputStrLen);
			WideCharToMultiByte(CP_ACP, 0, lpwszStrIn, nInputStrLen, pszOut, nOutputStrLen, 0, 0);
		}
	}

	return pszOut;
}

// 消息窗口的回调函数
INT_PTR CtrlPannelProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	int result = 0;
	TCHAR String[256] = { 0 };
	switch (message)
	{
	case WM_INITDIALOG:
	{
		CP->hWndPannel = hDlg;
		SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)CP->hicon);// 设置对话框的图标  
	}
	return (INT_PTR)TRUE;
	case WM_KEYDOWN:	// 键盘消息
	{
		switch (wParam)
		{
		case VK_F2:		// 关闭/打开控制台
			ShowWindow(CP->hWndPannel, IsWindowVisible(CP->hWndPannel) ? SW_HIDE : SW_SHOW);
			break;
		default:
			break;
		}
	}break;
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam), wmEvent = HIWORD(wParam);
		switch (wmId)
		{
		case IDCANCEL:
		{
			ShowWindow(CP->hWndPannel, SW_HIDE);
			return (INT_PTR)TRUE;
		}
		break;
		case IDC_CB_CAMERA:
		{
			if (wmEvent == CBN_SELCHANGE)
			{
				CB_Data.ID = (UINT)SendMessage(CP->hCombBox, CB_GETCURSEL, 0, 0L);
				SendMessage(CP->hCombBox, CB_GETLBTEXT, CB_Data.ID, (LPARAM)String);
				sprintf_s(CB_Data.Str , ConvertLPWSTRToLPSTR(String));
				SendMessage(CP->hWndParent, ContrlPannelMsg, (WPARAM)Camera_Change, (LPARAM)&CB_Data);				
			}
		}
			break;
		case IDC_CB_Fmt:
		{
			if (wmEvent == CBN_SELCHANGE)
			{
				CB_Data.ID = (UINT)SendMessage(CP->hCombBFmt, CB_GETCURSEL, 0, 0L);
				SendMessage(CP->hCombBFmt, CB_GETLBTEXT, CB_Data.ID, (LPARAM)String);
				sprintf_s(CB_Data.Str, ConvertLPWSTRToLPSTR(String));
				SendMessage(CP->hWndParent, ContrlPannelMsg, (WPARAM)Fmt_Change, (LPARAM)&CB_Data);
			}
		}
			break;
		case IDC_CHK_Focus:
			mFocus = IsDlgButtonChecked(CP->hWndPannel, IDC_CHK_Focus);
			SendMessage(CP->hWndParent, ContrlPannelMsg, (WPARAM)EnableFocus, (LPARAM)&mFocus);
			break;
		default:
			break;
		}
	}
	}
	return (INT_PTR)FALSE;
}


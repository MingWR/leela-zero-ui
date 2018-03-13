#pragma once
#include "utils.h"
#include <windows.h>
#include <tchar.h>
#include <vector>
#include <deque>
#include <array>
#include <functional>
#include <thread>
#include "../gtp_choice.h"


class BoardSpy : public GameAdvisor<GtpChoice> {
public:
	std::function<void()> onGtpReady;
	std::function<void()> onSizeChanged;
	std::function<void(int x, int y)> placeStone;
	

	BoardSpy();
	~BoardSpy();

	POINT coord2Screen(int x, int y) const;
	bool found() const;

	void initResource();
	void exit();

	void placeAt(int x, int y);

	bool bindWindow(HWND hWnd);

	bool routineCheck();
	
protected:
	bool initBitmaps();
	bool scanBoard(HWND hwnd, int data[], int& lastMove); 
	int detectStone(const BYTE* DIBS, int move, bool& isLastMove) const;
	double compareBoardRegionAt(const BYTE* DIBS, int move, const std::vector<BYTE>& stone, const std::vector<BYTE>& mask) const;

	bool locateStartPosition(Hdib&, int& startx, int& starty);
	bool calcBoardPositions(HWND hwnd, int startx, int starty);

protected:
	bool initialBoard(HWND hwnd);

	void init(const std::string& cfg_path);

private:

	int offsetX_;
	int offsetY_;
	int stoneSize_;

	int tplStoneSize_;
	int tplBoardSize_;

	std::vector<BYTE> blackStoneData_;
	std::vector<BYTE> whiteStoneData_;
	std::vector<BYTE> stoneMaskData_;
	std::vector<BYTE> lastMoveMaskData_;
	std::vector<BYTE> blackImage_;
	std::vector<BYTE> whiteImage_;

	int routineClock_;
	int placeStoneClock_;
	int placeX_;
	int placeY_;
	bool exit_;
	int placePos_;

private:
	std::array<int, 361> board_;
	std::array<int, 361> board_last_;
	int board_age[361];

private:
	// Window Resources
	HDC hDisplayDC_;
	int nDispalyBitsCount_;
	int nBpp_;

	Hdib boardDIB_;

	HBitmap hBoardBitmap_;
	Hdc hBoardDC_;

	HWND hTargetWnd;
	RECT lastRect_;
	int error_count_;
};



class PredictWnd {
public:
	PredictWnd(BoardSpy& spy);
	void create(HWND hParent);
	void show();
	void setPos(int movePos);
	void update();
	void hide();
	bool isVisible();

protected:
	bool loadRes();
	HRGN createRegion();

	static INT_PTR CALLBACK Proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	INT_PTR CALLBACK Proc(UINT uMsg, WPARAM wParam, LPARAM lParam);

	void onInit();

	std::vector<BYTE> label_mask_data;
	int move_;
	int size;
	HWND hWnd;

	static PredictWnd* current;

private:
	BoardSpy& spy_;

	bool mouseOver;
	bool hiding_;
};


class Dialog {

public:
	static Dialog* create(HINSTANCE hInst);
	
	static INT_PTR CALLBACK Proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);


	Dialog(HWND);
	~Dialog();
	void show(int nCmdShow);
	int messageLoop();

protected:
	void onInit();
	void onClose();
	void onDestroy();
	void onDetectInterval();
	void updateStatus();

protected:
	static Dialog* current;

	INT_PTR CALLBACK Proc(UINT uMsg, WPARAM wParam, LPARAM lParam);

	HWND hWnd_;
	BoardSpy spy;

	BOOL bStartSearchWindow;

	PredictWnd wndLabel_;

	TCHAR szMoves_[300 * 50];
	bool connected_;
	bool auto_play_;
};



extern TCHAR		g_szLocalPath[256];


void fatal(LPCTSTR msg);
bool loadBmpData(LPCTSTR pszFile, std::vector<BYTE>& img, int& width, int& height);


#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/photo.hpp>
#include <opencv2/imgproc/types_c.h>
#include <iostream>
#include <deque>
#include <cassert>

using namespace std;
using namespace cv;


class BlemishRemover
{
public:
	BlemishRemover(const char *windowName);
	BlemishRemover(const BlemishRemover&) = delete;
	BlemishRemover(BlemishRemover&&) = default;
	~BlemishRemover();

	void process(const char* inputFile);

	BlemishRemover& operator = (const BlemishRemover&) = delete;
	BlemishRemover& operator = (BlemishRemover&&) = delete;
	
private:

	//static constexpr const char windowName[] = "Blemish Removal";
	
	// Blemish size must be odd
    static constexpr int minBlemishSize = 5, maxBlemishSize = 99, defaultBlemishSize = 33, blemishSizeStep = 2;
	//static constexpr int minBlemishSize = 5, maxBlemishSize = 99, defaultBlemishSize = 33, blemishSizeStep = 2;
	///static constexpr int blemishSize = 25;	
    /*enum : int
    {
        minBlemishSize = 5, maxBlemishSize = 99, defaultBlemishSize = 33, blemishSizeStep = 2
    };*/

	static constexpr int maxUndoQueueLength = 10;

	static void onMouse(int event, int x, int y, int flags, void *data);

	void removeBlemish(int x, int y);

	double estimateSharpness(const Mat &roi, const Mat &mask) const;

	void saveState();

	bool undo();

	void reset();

	void welcome();

	void decorate(int x, int y);

	String windowName;
	Mat imSrc, imCur, imDecorated;
	deque<Mat> undoQueue;
	int blemishSize = defaultBlemishSize;
	bool intro = true;
	int lastMouseX = 0, lastMouseY = 0;
};	// BlemishRemover

// Before C++17 even a constexpr static data member must be initialized inside the class definition
constexpr int BlemishRemover::minBlemishSize, BlemishRemover::maxBlemishSize, BlemishRemover::defaultBlemishSize, BlemishRemover::blemishSizeStep;


BlemishRemover::BlemishRemover(const char *windowName)
	: windowName(windowName)
{
	namedWindow(this->windowName);

	//namedWindow(BlemishRemover::windowName);

	//setMouseCallback(BlemishRemover::windowName, onMouse, this);
}	// ctor

BlemishRemover::~BlemishRemover()
{
	destroyAllWindows();
}

void BlemishRemover::process(const char *inputFilePath)
{
	this->intro = true;

	this->imSrc = imread(inputFilePath, IMREAD_COLOR);
	CV_Assert(!this->imSrc.empty());

	setMouseCallback(this->windowName, BlemishRemover::onMouse, this);

	reset();
	welcome();

	for (int key = 0; (key & 0xFF) != 27; )		// exit on Escape
	{
		imshow(this->windowName, this->imDecorated);
		
		key = waitKey(10);

		if (this->intro)
		{
			if ((key & 0xFF) == 32)	// Space - leave the intro mode
			{
				this->intro = false;
				decorate(this->lastMouseX, this->lastMouseY);
			}	// start
		}	// intro
		else
		{
			if (key == 26)		// Ctrl+Z - undo
			{
				if (undo())	// undo
					decorate(this->lastMouseX, this->lastMouseY);	// draw a healing brush circle
				else
					cout << '\a';	// beep
			}	// undo
			else if ((key & 0xFF) == 'r' || (key & 0xFF) == 'R')	// r/R - reset
			{
				reset();	// reset images and the undo queue
				//welcome();
			}	// reset
		}	// !intro
	}	// for
}	// process


void BlemishRemover::onMouse(int event, int x, int y, int flags, void* data)
{
	BlemishRemover* br = static_cast<BlemishRemover*>(data);
	if (br->intro)
		return;

	switch (event)
	{
	case EVENT_LBUTTONUP:
		br->saveState();	// save current state to the undo queue
		br->removeBlemish(x, y);
		br->imCur.copyTo(br->imDecorated);
		break;

	case EVENT_MOUSEMOVE:
		br->decorate(x, y);
		br->lastMouseX = x;
		br->lastMouseY = y;
		break;

	case EVENT_MOUSEWHEEL:
		if (getMouseWheelDelta(flags) > 0)
			br->blemishSize = min(min(br->blemishSize + blemishSizeStep, br->maxBlemishSize), min(br->imCur.rows, br->imCur.cols));            
		else
			br->blemishSize = max(br->blemishSize - blemishSizeStep, br->minBlemishSize);

		br->decorate(x, y);		
		break;
	}	// switch
}	// onMouse

void BlemishRemover::removeBlemish(int x, int y)
{
	assert(!this->intro);

	// In order to be able to remove blemishes from image corners, we need to pad the image
	int padding = this->blemishSize / 2;
	Mat imPadded;
	copyMakeBorder(this->imCur, imPadded, padding, padding, padding, padding, BORDER_REFLECT | BORDER_ISOLATED);

	// Create a round mask for the selected rectangular patch
	Mat mask(this->blemishSize, this->blemishSize, CV_8UC1, Scalar(0));
	circle(mask, Point(this->blemishSize / 2, this->blemishSize / 2), this->blemishSize/2, Scalar(255), -1);
	
	// The direction arrays aid in calculating the centers of the neighboring regions
	/*static constexpr int ndirs = 4;		
	static constexpr int xdir[ndirs] = { -1, 0, +1, 0 }, ydir[ndirs] = { 0, -1, 0, +1 };*/
	static constexpr int ndirs = 8;
	static constexpr int xdir[ndirs] = { -1, -1, 0, +1, +1, +1, 0, -1 }, ydir[ndirs] = { 0, -1, -1, -1, 0, +1, +1, +1 };

	// Select the smoothest neighboring region
	Mat bestPatch;
	double minSharpness = numeric_limits<double>::max();

	for (int i = 0; i < ndirs; ++i)
	{
		int nx = padding + x + xdir[i] * this->blemishSize, ny = padding + y + ydir[i] * this->blemishSize;

		Range colRange(nx - this->blemishSize / 2, nx + this->blemishSize / 2 + 1)
			, rowRange(ny - this->blemishSize / 2, ny + this->blemishSize / 2 + 1);

		if (colRange.start < 0 || colRange.end > imPadded.cols || rowRange.start < 0 || rowRange.end > imPadded.rows)
			continue;

		const Mat &patch = imPadded(rowRange, colRange);
		double sharpness = estimateSharpness(patch, mask);

		if (sharpness < minSharpness)
		{
			minSharpness = sharpness;
			bestPatch = patch;
		}
	}	// for

	if (bestPatch.empty())
	{
		cout << "Unable to fix this blemish due to the lack of surrounding pixels." << '\a' << endl;
		return;
	}

	//// Create a round mask for the selected rectangular patch
	//Mat mask(bestPatch.size(), CV_8UC1, Scalar(0));
	//CV_Assert(mask.rows == blemishSize && mask.cols == blemishSize);
	//circle(mask, Point(bestPatch.cols / 2, bestPatch.rows / 2), blemishSize, Scalar(255), -1);

	// Replace the blemish region with the smoothest neighboring region
	Mat blend;
	seamlessClone(bestPatch, imPadded, mask, Point(padding + x, padding + y), blend, NORMAL_CLONE);
	this->imCur = blend(Rect(padding, padding, this->imCur.cols, this->imCur.rows));
}	// removeBlemish

double BlemishRemover::estimateSharpness(const Mat &roi, const Mat &mask) const
{
	// Compute gradients to measure smoothness (since the input image is 8-bit, the derivatives will be truncated)
	Mat gradx, grady;
	Sobel(roi, gradx, CV_16S, 1, 0, CV_SCHARR);		// Scharr filter that may give more accurate results than 3x3 Sobel
	Sobel(roi, grady, CV_16S, 0, 1, CV_SCHARR);
	//return sum(sum(abs(gradx)) + sum(abs(grady)))[0];

	Mat roiGrad;
	add(abs(gradx), abs(grady), roiGrad, mask);
	return sum(sum(roiGrad))[0];
}	// estimateSharpness

void BlemishRemover::saveState()
{
	assert(!this->intro);
	this->undoQueue.push_back(this->imCur.clone());
	if (this->undoQueue.size() > BlemishRemover::maxUndoQueueLength)
		this->undoQueue.pop_front();
}	// saveState

bool BlemishRemover::undo()
{
	assert(!this->intro);
	if (this->undoQueue.empty())
	{		
		return false;
	}
	else
	{
		this->undoQueue.back().copyTo(this->imCur);
		this->undoQueue.pop_back();
		return true;
	}
}	// undo

void BlemishRemover::reset()
{
	//this->intro = true;
	this->imSrc.copyTo(this->imCur);
	this->imCur.copyTo(this->imDecorated);
	this->undoQueue.clear();
	this->blemishSize = BlemishRemover::defaultBlemishSize;
}	// reset

void BlemishRemover::welcome()
{
	assert(this->intro);

	//this->imDecorated = Mat(this->imSrc.size(), CV_8UC3, Scalar(255,255,255));
	//this->imDecorated.setTo(Scalar(255,255,255));
	//this->imDecorated.setTo(Scalar(153,153,0));
	this->imDecorated.setTo(Scalar(21, 79, 241));
	//this->imDecorated.setTo(Scalar(0xDA, 0xED, 0xFC));
	//this->imDecorated.setTo(Scalar(255,255,255));
    
    
	static constexpr int nlines = 6;
	static constexpr char text[nlines][100] = {    // the compiler will warn in case the initializer string exceeds the fixed size
    //static constexpr string text[nlines] = {
		"Left click to remove a blemish",
		"Wheel to change the size of the healing brush",
		"Press Ctrl+Z to undo the last action",
		"Press R to reset",
		"Press Space to start",
		"Press Escape to quit"
	};

	static constexpr int fontFace = FONT_HERSHEY_COMPLEX_SMALL;
	static constexpr double fontScale = 0.8;

	int lineHeight = min(100, this->imDecorated.rows / nlines);
	//double fontScale = getFontScaleFromHeight(fontFace, lineHeight, 1);

	for (int i = 0; i < nlines; ++i)
	{
		//int baseLine;
		//const Size& sz = getTextSize(text[i], fontFace, fontScale, 1, &baseLine);
		const Size& sz = getTextSize(text[i], fontFace, fontScale, 1, nullptr);
		putText(this->imDecorated, text[i], Point(10, lineHeight*i+(lineHeight+sz.height)/2)
			//, fontFace, fontScale, Scalar(0x34,0x4E,0xEE), 1, LINE_AA);
			, fontFace, fontScale, Scalar(167, 241, 225), 1, LINE_AA);
			//, fontFace, fontScale, Scalar(167,241,225), 1, LINE_AA);
			//, fontFace, fontScale, Scalar(191,23,48), 1, LINE_AA);
			//, fontFace, fontScale, Scalar(2, 55, 189), 1, LINE_AA);
		//line(this->imDecorated, Point(0, lineHeight * (i+1)), Point(this->imCur.cols, lineHeight * (i+1)), Scalar(0, 0, 255));
	}	// for

	//imshow(BlemishRemover::windowName, this->imDecorated);
	//waitKey();
}	// welcome

void BlemishRemover::decorate(int x, int y)
{
	assert(!this->intro);
	this->imCur.copyTo(this->imDecorated);

	// Don't draw the circle if the mouse cursor is leaving the window. Unfortunately, OpenCV doesn't provide a mouse leave event.
	// This code is used as a workaround, so we check whether the mouse position is near the border.  
	int margin = 14;
	if (x > margin && x < this->imCur.cols-margin && y > margin && y < this->imCur.rows-margin)
		circle(this->imDecorated, Point(x, y), this->blemishSize/2, Scalar(233, 233, 233), 1);

	//// Don't draw the circle if it exceeds image boundaries
	//int r = this->blemishSize / 2;
	////int r = BlemishRemover::blemishSize / 2;
	//if (x-r >= 0 && x+r < this->imDecorated.cols && y-r >= 0 && y+r < this->imDecorated.rows)
	//	circle(this->imDecorated, Point(x, y), r, Scalar(233, 233, 233), 1);
}	// decorate



int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		cout << "Usage: reblem <input image>" << endl;
		return -1;
	}

	try
	{
		BlemishRemover blemishRemover("Blemish Removal");		
		blemishRemover.process(argv[1]);
	}
	catch (const std::exception& e)		// cv::Exception inherits std::exception
	{
		cerr << e.what() << endl;
		return -2;
	}
		

	return 0;
}

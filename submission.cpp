
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/photo.hpp>
#include <opencv2/imgproc/types_c.h>
#include <iostream>
#include <deque>

using namespace std;
using namespace cv;


class BlemishRemover
{
public:
	BlemishRemover();
	// TODO: define copy and move constructors along with assignment and move-assignment operators

	~BlemishRemover();

	void process(const char* inputFile);

	
private:

	static constexpr const char windowName[] = "Blemish Removal";
	static constexpr int maxUndoQueueLength = 10;
	// Blemish size must be odd
	//static constexpr int blemishSize = 25;	
	static constexpr int minBlemishSize = 5, maxBlemishSize = 99, defaultBlemishSize = 33, blemishSizeStep = 2;

	static void onMouse(int event, int x, int y, int flags, void *data);

	void removeBlemish(int x, int y);

	double estimateSharpness(const Mat &roi, const Mat &mask) const;

	void saveState();

	bool undo();

	void reset();

	void decorate(int x, int y);

	Mat imSrc, imCur, imDecorated;
	deque<Mat> undoQueue;
	int blemishSize = defaultBlemishSize;
	int lastMouseX = 0, lastMouseY = 0;
};	// BlemishRemover


BlemishRemover::BlemishRemover()
{
	//static_assert(BlemishRemover::blemishSize > 0 && BlemishRemover::blemishSize % 2 == 1, "Blemish size must be a positive odd integer.");

	namedWindow(BlemishRemover::windowName);

	setMouseCallback(BlemishRemover::windowName, onMouse, this);
}	// ctor

BlemishRemover::~BlemishRemover()
{
	destroyAllWindows();
}

void BlemishRemover::process(const char *inputFilePath)
{
	this->imSrc = imread(inputFilePath, IMREAD_COLOR);
	CV_Assert(!this->imSrc.empty());
	
	reset();
	
	for (int key = 0; (key & 0xFF) != 27; )		// exit on Escape
	{
		imshow(BlemishRemover::windowName, this->imDecorated);
		//imshow(BlemishRemover::windowName, imCur);
		
		key = waitKey(10);

		if (key == 26)		// Ctrl+Z
		{
			if (undo())	// undo
				decorate(this->lastMouseX, this->lastMouseY);	// draw a healing brush circle
			else
				cout << '\a';	// beep
		}	// undo
		else if ((key & 0xFF) == 'r' || (key & 0xFF) == 'R')
		{
			reset();	// reset images and the undo queue
		}	// reset
	}	// for
}	// process


void BlemishRemover::onMouse(int event, int x, int y, int flags, void* data)
{
	BlemishRemover* br = static_cast<BlemishRemover*>(data);

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
			br->blemishSize = min(br->blemishSize + blemishSizeStep, br->maxBlemishSize);
		else
			br->blemishSize = max(br->blemishSize - blemishSizeStep, br->minBlemishSize);

		br->decorate(x, y);		
		break;
	}	// switch
}	// onMouse

void BlemishRemover::removeBlemish(int x, int y)
{
	// In order to be able to remove blemishes from image corners, we need to pad the image
	int padding = this->blemishSize / 2;
	//static constexpr int padding = BlemishRemover::blemishSize / 2;
	Mat imPadded;
	copyMakeBorder(this->imCur, imPadded, padding, padding, padding, padding, BORDER_REFLECT | BORDER_ISOLATED);

	// Create a round mask for the selected rectangular patch
	Mat mask(blemishSize, blemishSize, CV_8UC1, Scalar(0));
	circle(mask, Point(blemishSize / 2, blemishSize / 2), blemishSize/2, Scalar(255), -1);
	//cout << mask << endl;

	//// Compute gradients to measure smoothness (since the input image is 8-bit, the derivatives will be truncated)
	//Mat gradX, gradY;
	//Sobel(imPadded, gradX, CV_16S, 1, 0, CV_SCHARR);	// Scharr filter that may give more accurate results than 3x3 Sobel
	//Sobel(imPadded, gradY, CV_16S, 0, 1, CV_SCHARR);
	//Mat absGrad = abs(gradX) + abs(gradY);
	////cout << absGrad << endl;

	// The direction arrays aid in calculating the centers of the neighboring regions
	static constexpr int ndirs = 4;		// TODO: perhaps, use 8 directions?
	static constexpr int xdir[ndirs] = { -1, 0, +1, 0 }, ydir[ndirs] = { 0, -1, 0, +1 };

	// Select the smoothest neighboring region
	Mat bestPatch;
	double minSharpness = numeric_limits<double>::max();

	for (int i = 0; i < ndirs; ++i)
	{
		int nx = padding + x + xdir[i] * blemishSize, ny = padding + y + ydir[i] * blemishSize;

		Range colRange(nx - blemishSize / 2, nx + blemishSize / 2 + 1)
			, rowRange(ny - blemishSize / 2, ny + blemishSize / 2 + 1);

		if (colRange.start < 0 || colRange.end > imPadded.cols || rowRange.start < 0 || rowRange.end > imPadded.rows)
			continue;

		const Mat &patch = imPadded(rowRange, colRange);
		//Mat patchgrad1 = absGrad(rowRange, colRange);
		//cout << patchgrad1 << endl;

		double sharpness = estimateSharpness(patch, mask);

		//double sharpness = sum(sum(absGrad(rowRange, colRange)))[0];
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
	//cout << blend.type() << endl;
	/*Mat tmp = bestPatch.clone();
	cout << tmp.size() << endl;
	seamlessClone(tmp, imPadded, mask, Point(padding + x, padding + y), blend, NORMAL_CLONE);*/
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
	this->undoQueue.push_back(this->imCur.clone());
	if (this->undoQueue.size() > BlemishRemover::maxUndoQueueLength)
		this->undoQueue.pop_front();
}	// saveState

bool BlemishRemover::undo()
{
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
	this->imSrc.copyTo(this->imCur);
	this->imCur.copyTo(this->imDecorated);
	this->undoQueue.clear();
	this->blemishSize = BlemishRemover::defaultBlemishSize;
}	// reset

void BlemishRemover::decorate(int x, int y)
{
	this->imCur.copyTo(this->imDecorated);

	// TODO: try checking just one border pixel
	// Don't draw the circle if it exceeds image boundaries
	int r = BlemishRemover::blemishSize / 2;
	//int r = BlemishRemover::blemishSize / 2;
	if (x-r >= 0 && x+r < this->imDecorated.cols && y-r >= 0 && y+r < this->imDecorated.rows)
		circle(this->imDecorated, Point(x, y), r, Scalar(233, 233, 233), 1);
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
		BlemishRemover blemishRemover;
		blemishRemover.process(argv[1]);
	}
	catch (const std::exception& e)		// cv::Exception inherits std::exception
	{
		cerr << e.what() << endl;
		return -2;
	}
		

	return 0;
}
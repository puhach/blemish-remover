
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/photo.hpp>
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
	static constexpr int blemishSize = 25;	

	static void onMouse(int event, int x, int y, int flags, void *data);

	void removeBlemish(int x, int y);

	double calcSharpness(const Mat &roi);

	void saveState();

	bool undo();

	Mat imSrc, imCur;
	deque<Mat> undoQueue;
};	// BlemishRemover


BlemishRemover::BlemishRemover()
{
	static_assert(BlemishRemover::blemishSize > 0 && BlemishRemover::blemishSize % 2 == 1, "Blemish size must be a positive odd integer.");

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
	
	this->imSrc.copyTo(this->imCur);
	
	for (int key = 0; (key & 0xFF) != 27; )		// exit on Escape
	{
		imshow(BlemishRemover::windowName, imCur);
		
		key = waitKey(10);

		if (key == 26)		// Ctrl+Z
		{
			undo();	// undo
		}	// undo
		else if ((key & 0xFF) == 'r' || (key & 0xFF) == 'R')
		{
			// Reset
			this->imSrc.copyTo(this->imCur);
		}	// reset
	}	// for
}	// process


void BlemishRemover::onMouse(int event, int x, int y, int flags, void* data)
{
	BlemishRemover* br = static_cast<BlemishRemover*>(data);

	switch (event)
	{
	case EVENT_LBUTTONUP:

		// Save current state to the undo queue
		br->saveState();
		//circle(br->imCur, Point(x,y), BlemishRemover::blemishSize, Scalar(0,255,0), -1);
		br->removeBlemish(x, y);
		break;
	}	// switch
}	// onMouse

void BlemishRemover::removeBlemish(int x, int y)
{
	// In order to be able to remove blemishes from image corners, we need to pad the image

	static constexpr int padding = BlemishRemover::blemishSize / 2;
	Mat imPadded;
	copyMakeBorder(this->imCur, imPadded, padding, padding, padding, padding, BORDER_REFLECT | BORDER_ISOLATED);

	// Compute gradients to measure smoothness (since the input image is 8-bit, the derivatives will be truncated)
	Mat gradX, gradY;
	Sobel(imPadded, gradX, CV_16S, 1, 0, 3);	// TODO: try using Scharr
	Sobel(imPadded, gradY, CV_16S, 0, 1, 3);
	Mat absGrad = abs(gradX) + abs(gradY);
	//cout << absGrad << endl;

	// The direction arrays aid in calculating the starting row/column of the neighbor regions
	static constexpr int ndirs = 4;
	//static constexpr int xdir[ndirs] = { -3, -1, +1, -1 }, ydir[ndirs] = { -1, -3, -1, +1 };
	static constexpr int xdir[ndirs] = { -1, 0, +1, 0 }, ydir[ndirs] = { 0, -1, 0, +1 };

	Mat bestPatch;
	double minSharpness = numeric_limits<double>::max();

	for (int i = 0; i < ndirs; ++i)
	{
		int nx = padding + x + xdir[i] * blemishSize, ny = padding + y + ydir[i] * blemishSize;

		Range colRange(nx - blemishSize / 2, nx + blemishSize / 2 + 1)
			, rowRange(ny - blemishSize / 2, ny + blemishSize / 2 + 1);

		if (colRange.start < 0 || colRange.end > imPadded.cols || rowRange.start < 0 || rowRange.end > imPadded.rows)
			continue;

		double sharpness = sum(sum(absGrad(rowRange, colRange)))[0];
		if (sharpness < minSharpness)
		{
			minSharpness = sharpness;
			bestPatch = imPadded(rowRange, colRange);
		}
	}	// for

	if (bestPatch.empty())
	{
		cout << "Unable to fix this blemish due to the lack of surrounding pixels." << endl;
		return;
	}

	Mat blend;
	seamlessClone(bestPatch, imPadded, Mat(bestPatch.size(), CV_8UC1, Scalar(255)), Point(padding+x,padding+y), blend, NORMAL_CLONE);	
	//seamlessClone(bestPatch, this->imCur, Mat(bestPatch.size(), CV_8UC1, Scalar(255)), Point(x, y), blend, NORMAL_CLONE);
	this->imCur = blend(Rect(padding, padding, this->imCur.cols, this->imCur.rows));
}	// removeBlemish

//double BlemishRemover::calcSharpness(const Mat &roi) const
//{
//	Mat roiGray;
//	Mat gradx, grady;
//	Sobel(roi, gradx, CV_64F, 1, 0, 3);
//	Sobel(roi, grady, CV_64F, 0, 1, 3);
//	return sum(abs(gradx)) + sum(abs(grady));
//}	// calcSharpness

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
		cout << '\a';	// beep
		return false;
	}
	else
	{
		this->undoQueue.back().copyTo(this->imCur);
		this->undoQueue.pop_back();
		return true;
	}
}	// undo

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		cout << "Usage: reblemish <input image>" << endl;
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
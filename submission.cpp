
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
	static constexpr int blemishSize = 5;	

	static void onMouse(int event, int x, int y, int flags, void *data);

	void removeBlemish(int x, int y);

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
	//const Mat &roi = this->imCur(Range(this->imCur.rows-1, 0), Range(0, 20));
	//cout << roi.size() << endl;

	// In order to be able to remove blemishes from image corners, we need to pad the image

	static constexpr int padding = BlemishRemover::blemishSize / 2;
	Mat imPadded;
	copyMakeBorder(this->imCur, imPadded, padding, padding, padding, padding, BORDER_REFLECT | BORDER_ISOLATED);


	// The direction arrays aid in calculating the starting row/column of the neighbor regions
	static constexpr int ndirs = 4;
	static constexpr int xdir[ndirs] = { -3, -1, +1, -1 }, ydir[ndirs] = { -1, -3, -1, +1 };

	Mat bestPatch;
	double minSharpness = numeric_limits<double>::max();

	for (int i = 0; i < ndirs; ++i)
	{
		int xcorr = static_cast<int>(xdir[i] > 0);
		int ycorr = static_cast<int>(ydir[i] > 0);
		int x1 = padding + x + xdir[i] * blemishSize / 2 + xcorr, x2 = x1 + blemishSize;
		int y1 = padding + y + ydir[i] * blemishSize / 2 + ycorr, y2 = y1 + blemishSize;

		if (x1 < 0 || x2 > imPadded.cols || y1 < 0 || y2 > imPadded.rows)
			continue;

		//x1 = max(0, x1);
		//x2 = min(x2, this->imCur.cols);
		//y1 = max(0, y1);
		//y2 = min(y2, this->imCur.rows);

		const Mat& roi = imPadded(Range(y1, y2), Range(x1, x2));
		cout << roi.size() << endl;

		double sharpness = calcSharpness(roi);
		if (sharpness < minSharpness)
		{
			minSharpness = sharpness;
			bestPatch = roi;
		}
	}	// for


	if (bestPatch.empty())
	{
		cout << "Unable to fix this blemish due to the lack of surrounding pixels." << endl;
		return;
	}

	seamlessClone(this->imCur, bestPatch, Mat(bestPatch.size(), CV_8UC1, Scalar(255)), Point(x,y), this->imCur, NORMAL_CLONE);

	/*Range lrange(x - blemishSize/2 - blemishSize, x - blemishSize/2)
		, rrange(x + blemishSize/2 + 1, x + blemishSize/2 + blemishSize + 1)
		, urange(x - blemishSize/2, x + blemishSize/2 + 1);


	int t1 = lrange.empty();
	int t2 = lrange.size();*/
	//static constexpr int dirLen = 4;
	//static const int dir[dirLen][2] = { {-1, 0}, {0, -1}, {+1, 0}, {0, +1} };

	//static const array<array<int, 2>, 4> dir{ { {-1, 0}, { 0, -1 }, { +1, 0 }, { 0, +1 } } };

	//constexpr int w = BlemishRemover::blemishSize;

	//for (int i = 0; i < dir.size(); ++i)
	//{
	//	
	//	int fromCol = x + dir[i][0] * BlemishRemover::blemishSize/2
	//	  , toCol = x + dir[i][0] * (BlemishRemover::blemishSize/2 + BlemishRemover::blemishSize)
	//	  , fromRow = y + dir[i][1] * BlemishRemover::blemishSize/2
	//	  , toRow = y + dir[i][1] * (BlemishRemover::blemishSize/2 + BlemishRemover::blemishSize);

	//	if (fromCol > toCol)
	//	{
	//		swap(fromCol, toCol);
	//		++fromCol;
	//		++toCol;
	//	}

	//	if (fromRow > toRow)
	//	{
	//		swap(fromRow, toRow);
	//		++fromRow;
	//		++toRow;
	//	}
	//}	// for
	
	
}	// removeBlemish

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
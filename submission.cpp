
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
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

	static void onMouse(int event, int x, int y, int flags, void *data);

	void saveState();
	bool undo();

	//string inputFile;
	Mat imSrc, imCur;
	deque<Mat> undoQueue;
};	// BlemishRemover


BlemishRemover::BlemishRemover()
{
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
	
	//this->imPrev = this->imSrc.clone();
	//this->imSrc.copyTo(this->imPrev);
	this->imSrc.copyTo(this->imCur);
	
	for (int key = 0; (key & 0xFF) != 27; )
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
		circle(br->imCur, Point(x,y), 5, Scalar(0,255,0), -1);
		break;
	}	// switch
}	// onMouse



void BlemishRemover::saveState()
{
	//br->imCur.copyTo(br->imPrev);
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
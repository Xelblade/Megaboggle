#include "Solver.h"

//Static variables
std::thread SolverThreadPool::mThreads[NUM_THREADS];
void(*SolverThreadPool::mFnc)(Search *);
//Theoretically there's a race condition where more than MIN_INT threads
//decrement at the same time and it overflows to max int
std::atomic<int> SolverThreadPool::mNumWorkItems;

Search::Search(Dictionary* dictionary, DictionaryNode* dNode, const Board* board, unsigned int bIndex) :
    mDNode(dNode),
    mBIndex(bIndex),
    mBoard(board),
    mDictionary(dictionary)
{
    mVisited = new std::vector<unsigned int>();
}

Search::~Search()
{
    delete mVisited;
}

SolverThreadPool::SolverThreadPool(void(*fnc)(Search *), int numWorkItems)
{
    SolverThreadPool::mFnc = fnc;
    SolverThreadPool::mNumWorkItems = numWorkItems;
}

void SolverThreadPool::start(Dictionary* dictionary, const Board* board)
{
    for (unsigned int i = 0; i < NUM_THREADS; ++i)
    {
        SolverThreadPool::mThreads[i] = std::thread(SolverThreadPool::startSolverWorker, dictionary, board);
    }
}

void SolverThreadPool::join()
{
    for (unsigned int i = 0; i < NUM_THREADS; ++i)
    {
        SolverThreadPool::mThreads[i].join();
    }
}

void SolverThreadPool::startSolverWorker(Dictionary* dictionary, const Board* board)
{
    DictionaryNode* root = dictionary->getRoot();
    //Re-use this vector so we only have to allocate once
    Search* search = new Search(dictionary, root, board, 0);

    //We exit when the work queue is empty
    while (true)
    {
        //Pull a row to process from the work queue
        int row = --SolverThreadPool::mNumWorkItems;
        //We decrement first so we get execute on every element from [0, mNumWorkItems - 1]
        if (row < 0) {
            delete search;
            return;
        }

        if (BY_ROW) {
            //Solve for every element in that row
            for (unsigned int i = 0; i < board->mWidth; ++i)
            {
                unsigned int bIndex = row * board->mWidth + i;

                //Set up and run the search
                search->mDNode = root;
                search->mBIndex = bIndex;
                SolverThreadPool::mFnc(search);
            }
        }
        else
        {
            //Set up and run the search for an individual element
            search->mDNode = root;
            search->mBIndex = row;
            SolverThreadPool::mFnc(search);
        }
    }

    delete search;
}

Solver::Solver(Dictionary* dictionary, const Board* board) :
    mDictionary(dictionary),
    mBoard(board)
{
    //Chop up work to put in the work queue
    if (BY_ROW) {
        mThreadPool = new SolverThreadPool(Solver::recursiveSearch, board->mHeight);
    }
    else
    {
        mThreadPool = new SolverThreadPool(Solver::recursiveSearch, board->mWidth * board->mHeight);
    }
}

Solver::~Solver()
{
    delete mThreadPool;
}

void Solver::solve()
{
    //Start the thread pool on the work queue
    mThreadPool->start(mDictionary, mBoard);
    mThreadPool->join();
}

void Solver::recursiveSearch(Search* search)
{
    unsigned int oldBIndex = search->mBIndex;

    //Already using this board node as part of our word
    if (indexVisited(oldBIndex, search->mVisited))
    {
        return;
    }

    DictionaryNode* oldDNode = search->mDNode;
    search->mDNode = oldDNode->mChildren[Dictionary::charToIndex(search->mBoard->mBoard[oldBIndex])];
    //No dictionary entries remain along this path; either never existed or was disabled
    if (!search->mDNode || search->mDNode->mIsDisabled)
    {
        search->mDNode = oldDNode;
        return;
    }

    //Add the current letter to the word we're building
    search->mVisited->push_back(oldBIndex);

    //Check if we found a new word
    if (search->mDNode->mIsWord && !search->mDNode->mIsFound)
    {
        //Found a word! Check if we need to remove it from the dictionary
        search->mDNode->mIsFound = true;
        Dictionary::removeWord(search->mDNode);
    }

    unsigned int x = oldBIndex % search->mBoard->mWidth;
    unsigned int y = oldBIndex / search->mBoard->mWidth;
    bool hasLeft = x > 0;
    bool hasRight = x < search->mBoard->mWidth - 1;
    bool hasUp = y > 0;
    bool hasDown = y < search->mBoard->mHeight - 1;

    //Look left
    if (hasLeft) {
        search->mBIndex = oldBIndex - 1;
        recursiveSearch(search);

        //Look up-left
        if (hasUp) {
            search->mBIndex = oldBIndex - 1 - search->mBoard->mWidth;
            recursiveSearch(search);
        }

        //Look down-left
        if (hasDown) {
            search->mBIndex = oldBIndex - 1 + search->mBoard->mWidth;
            recursiveSearch(search);
        }
    }

    //Look right
    if (hasRight) {
        search->mBIndex = oldBIndex + 1;
        recursiveSearch(search);

        //Look up-right
        if (hasUp) {
            search->mBIndex = oldBIndex + 1 - search->mBoard->mWidth;
            recursiveSearch(search);
        }

        //Look down-right
        if (hasDown) {
            search->mBIndex = oldBIndex + 1 + search->mBoard->mWidth;
            recursiveSearch(search);
        }
    }

    //Look up
    if (hasUp) {
        search->mBIndex = oldBIndex - search->mBoard->mWidth;
        recursiveSearch(search);
    }

    //Look down
    if (hasDown) {
        search->mBIndex = oldBIndex + search->mBoard->mWidth;
        recursiveSearch(search);
    }
    
    search->mBIndex = oldBIndex;
    search->mDNode = oldDNode;
    search->mVisited->pop_back();
}

inline bool Solver::indexVisited(unsigned int bIndex, std::vector<unsigned int>* visited)
{
    for (unsigned int i = 0; i < visited->size(); ++i)
    {
        if (bIndex == (*visited)[i])
        {
            return true;
        }
    }
    return false;
}

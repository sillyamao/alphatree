//
// Created by yanyu on 2017/7/12.
//
#ifndef ALPHATREE_ALPHATREE_H
#define ALPHATREE_ALPHATREE_H

#include "basealphatree.h"
#include "../base/threadpool.h"
#include "../base/dcache.h"
#include "../base/randomchoose.h"
#include <math.h>

//#define MAX_PROCESS_STR_LEN 4096 * 64
class NodeCache {
public:
    NodeCache() {
        cache = new char[cacheSize];
    }

    ~NodeCache() {
        delete[]cache;
    }

    char *cache;
    static size_t cacheSize;
};

size_t NodeCache::cacheSize = GET_ELEMEMT_SIZE(HISTORY_DAYS, SAMPLE_DAYS) *
                              STOCK_SIZE * sizeof(float);

class AlphaCache {
public:
    AlphaCache() {
        nodeCacheSize = MAX_NODE_BLOCK;
        nodeRes = new std::shared_future<int>[nodeCacheSize];
        result = DCache<NodeCache>::create();
        dayCacheSize = MAX_NODE_BLOCK * GET_ELEMEMT_SIZE(HISTORY_DAYS, SAMPLE_DAYS);
        dayFlag = new CacheFlag[dayCacheSize];
        codeCacheSize = STOCK_SIZE * CODE_LEN;
        codes = new char[codeCacheSize];
    }

    ~AlphaCache() {
        delete[]nodeRes;
        DCache<NodeCache>::release(result);
        delete[]dayFlag;
        delete[]codes;
    }

    void initialize(size_t nodeSize, size_t historyDays, size_t dayBefore, size_t sampleDays, const char *codes,
                    size_t stockSize) {
        const char *curSrcCode = codes;
        char *curDstCode = this->codes;
        for (size_t i = 0; i < stockSize; ++i) {
            strcpy(curDstCode, curSrcCode);
            int codeLen = strlen(curSrcCode);
            curSrcCode += (codeLen + 1);
            curDstCode += (codeLen + 1);
        }
        initialize(nodeSize, historyDays, dayBefore, sampleDays, stockSize);
    }

    void initialize(size_t nodeSize, size_t historyDays, AlphaDB *alphaDatabase) {
        size_t sampleDays = alphaDatabase->getDays() - historyDays + 1;
        initialize(nodeSize, historyDays, 0, sampleDays, alphaDatabase->getStockNum());
        this->stockSize = alphaDatabase->getAllCodes(this->codes);
    }


    //监控某个节点在多线程中的计算状态
    std::shared_future<int> *nodeRes = {nullptr};
    //保存中间计算结果
    DCache<NodeCache> *result = {nullptr};
    //保存某日所有股票是否需要计算
    CacheFlag *dayFlag = {nullptr};
    //保存需要计算的股票代码
    char *codes = {nullptr};
    //记录各个缓存当前大小,如果某个计算要求的大小超过了就需要重新分配内存
    size_t nodeCacheSize = {0};
    size_t dayCacheSize = {0};
    size_t codeCacheSize = {0};

    size_t dayBefore = {0};
    size_t sampleDays = {0};
    size_t stockSize = {0};

protected:
    void initialize(size_t nodeSize, size_t historyDays, size_t dayBefore, size_t sampleDays, size_t stockSize) {
        this->dayBefore = dayBefore;
        this->sampleDays = sampleDays;
        this->stockSize = stockSize;

        if (nodeSize > nodeCacheSize) {
            delete[]nodeRes;
            //delete[]nodeFlag;
            nodeRes = new std::shared_future<int>[nodeSize];
            //nodeFlag = new bool[nodeSize];
            nodeCacheSize = nodeSize;
        }

        size_t scs = GET_ELEMEMT_SIZE(historyDays, sampleDays) * stockSize * sizeof(float);
        if (scs > NodeCache::cacheSize) {
            DCache<NodeCache>::release(result);
            NodeCache::cacheSize = scs;
            result = DCache<NodeCache>::create();
        }

        size_t dcs = nodeSize * GET_ELEMEMT_SIZE(historyDays, sampleDays);
        if (dcs > dayCacheSize) {
            delete[]dayFlag;

            dayFlag = new CacheFlag[dcs];
            dayCacheSize = dcs;
        }

        size_t ccs = stockSize * CODE_LEN;
        if (ccs > codeCacheSize) {
            delete[]this->codes;
            this->codes = new char[ccs];
            codeCacheSize = ccs;
        }

        //更新某个日期是否需要计算的标记
        memset(dayFlag, 0, dcs * sizeof(CacheFlag));
        result->releaseAll();
    }
};


class AlphaTree : public BaseAlphaTree {
public:
    AlphaTree() : BaseAlphaTree(), maxHistoryDay_(NONE) {}

    virtual void clean() {
        maxHistoryDay_ = NONE;
        BaseAlphaTree::clean();
    }

    int getMaxHistoryDays() {
        if (maxHistoryDay_ == NONE) {
            for (auto i = 0; i < subtreeList_.getSize(); ++i) {
                int historyDays = getMaxHistoryDays(subtreeList_[i].rootId) + 1;
                if (historyDays > maxHistoryDay_)
                    maxHistoryDay_ = historyDays;
            }
            //cout<<"md:"<<maxHistoryDay_<<endl;
        }
        return maxHistoryDay_;
    }


    //计算alpha并返回,注意返回的是全部数据,要想使用必须加上偏移res + (int)((alphaTree->getHistoryDays() - 1) * stockSize
    void calAlpha(AlphaDB *alphaDataBase, size_t dayBefore, size_t sampleSize, const char *codes, size_t stockSize,
                  AlphaCache *cache, ThreadPool *threadPool) {
        maxHistoryDay_ = NONE;
        //如果缓存空间不够,就重新申请内存
        cache->initialize(nodeList_.getSize(), getMaxHistoryDays(), dayBefore, sampleSize, codes, stockSize);

        //重新标记所有节点
        flagAllNode(cache);

        //int rootId = getSubtreeRootId(rootName);
        /*for (auto i = 0; i < nodeList_.getSize(); ++i) {
            int nodeId = i;
            AlphaTree *alphaTree = this;
            //cout<<i<<endl;
            cache->nodeRes[i] = threadPool->enqueue([alphaTree, alphaDataBase, nodeId, cache] {
                return alphaTree->cast(alphaDataBase, nodeId, cache);
            }).share();
        }*/
        calAlpha(alphaDataBase, cache, threadPool);

    }


    //保存alphatree到alphaDB,isFeature表示是否作为机器学习的特征
    void cacheAlpha(AlphaDB *alphaDataBase, AlphaCache *cache, ThreadPool *threadPool, bool isToFile) {
        maxHistoryDay_ = NONE;
        cache->initialize(nodeList_.getSize(), getMaxHistoryDays(), alphaDataBase);
        //int dateSize = alphaDataBase->getDays();
        //重新标记所有节点
        flagAllNode(cache);
        //flagAllStock(alphaDataBase, cache, threadPool, true);
        calAlpha(alphaDataBase, cache, threadPool);
        for (auto i = 0; i < subtreeList_.getSize(); ++i) {
            int memid = cache->nodeRes[subtreeList_[i].rootId].get();
            float *alpha = (float *) cache->result->getCacheMemory(memid).cache;
            //bool *flag = cache->flagRes[subtreeList_[i].rootId].get();
            int needDay = getMaxHistoryDays() - 1;
            //cout<<"set "<<subtreeList_[i].name<<endl;
            alphaDataBase->setElement(subtreeList_[i].name, needDay, alpha, isToFile);
        }
        //cout<<"fc!"<<endl;
    }


    float optimizeAlpha(const char *rootName, AlphaDB *alphaDataBase, size_t dayBefore, size_t sampleSize, const char *codes, size_t stockSize,
                       AlphaCache *cache, ThreadPool *threadPool, float exploteRatio = 0.1f, int errTryTime = 64) {
        float* bestCoffList = new float[coffList_.getSize()];

        for(int i = 0; i < coffList_.getSize(); ++i){
            bestCoffList[i] = coffList_[i].coffValue;
        }

        calAlpha(alphaDataBase, dayBefore, sampleSize, codes, stockSize, cache, threadPool);
        float bestRes = getAlpha(rootName, cache)[0];

        RandomChoose rc = RandomChoose(2 * coffList_.getSize());

        auto curErrTryTime = errTryTime;
        while (curErrTryTime > 0){
            //修改参数
            float lastCoffValue = NAN;
            int curIndex = 0;
            bool isAdd = false;
            while(lastCoffValue == NAN){
                curIndex = rc.choose();
                isAdd = curIndex < coffList_.getSize();
                curIndex = curIndex % coffList_.getSize();
                int srcIndex = coffList_[curIndex].srcNodeIndex;
                if(isAdd && coffList_[curIndex].coffValue < nodeList_[srcIndex].getElement()->getMaxCoff()){
                    lastCoffValue = coffList_[curIndex].coffValue;
                    if(nodeList_[srcIndex].getCoffUnit() == CoffUnit::COFF_VAR){
                        coffList_[curIndex].coffValue += 0.016f;
                    } else {
                        coffList_[curIndex].coffValue += 1;
                    }
                    coffList_[curIndex].coffValue = min(coffList_[curIndex].coffValue, nodeList_[srcIndex].getElement()->getMaxCoff());
                }
                if(!isAdd && coffList_[curIndex].coffValue > nodeList_[srcIndex].getElement()->getMinCoff()){
                    lastCoffValue = coffList_[curIndex].coffValue;
                    if(nodeList_[srcIndex].getCoffUnit() == CoffUnit::COFF_VAR){
                        coffList_[curIndex].coffValue -= 0.016f;
                    } else {
                        coffList_[curIndex].coffValue -= 1;
                    }
                    coffList_[curIndex].coffValue = max(coffList_[curIndex].coffValue, nodeList_[srcIndex].getElement()->getMinCoff());
                }
            }

            calAlpha(alphaDataBase, dayBefore, sampleSize, codes, stockSize, cache, threadPool);
            float res = getAlpha(rootName, cache)[0];
            if(res > bestRes){
                curErrTryTime = errTryTime;
                bestRes = res;
                for(int i = 0; i < coffList_.getSize(); ++i){
                    bestCoffList[i] = coffList_[i].coffValue;
                }
                //根据当前情况决定调整该参数的概率
                curIndex = isAdd ? curIndex : coffList_.getSize() + curIndex;
                rc.add(curIndex);
            } else{
                --curErrTryTime;
                if(!rc.isExplote(exploteRatio)){
                    //恢复现场
                    coffList_[curIndex].coffValue = lastCoffValue;
                }
                curIndex = isAdd ? curIndex : coffList_.getSize() + curIndex;
                rc.reduce(curIndex);
            }

        }

        for(int i = 0; i < coffList_.getSize(); ++i){
            coffList_[i].coffValue = bestCoffList[i];
        }
        delete []bestCoffList;
        return bestRes;
    }


    const float *getAlpha(const char *rootName, AlphaCache *cache) {
        return getAlpha(getSubtreeRootId(rootName), cache);
    }

    const char *getProcess(const char *rootName, AlphaCache *cache) {
        return getProcess(getSubtreeRootId(rootName), cache);
    }


    //读取已经计算好的alpha
    const float *getAlpha(int nodeId, AlphaCache *cache) {
        //int dateSize = GET_ELEMEMT_SIZE(getMaxHistoryDays(), cache->sampleDays);
        //float* result = AlphaTree::getNodeCacheMemory(nodeId, dateSize, stockSize, resultCache);
        int memid = cache->nodeRes[nodeId].get();
        float *result = (float *) cache->result->getCacheMemory(memid).cache;
        //CacheFlag* flag = AlphaTree::getNodeCacheMemory(nodeId, dateSize, cache->stockSize, cache->stockFlag);
        //flagResult(result, flag, dateSize * cache->stockSize);
        return result + (int) ((getMaxHistoryDays() - 1) * cache->stockSize);
    }

    const char *getProcess(int nodeId, AlphaCache *cache) {
        return (char *) cache->result->getCacheMemory(cache->nodeRes[nodeId].get()).cache;
    }

    /*const int* getSign(AlphaCache* cache){
        return cache->sign + (int)((getMaxHistoryDays() - 1) * cache->stockSize);
    }*/

    static const float *getAlpha(const float *res, size_t sampleIndex, size_t stockSize) { return res + (sampleIndex *
                                                                                                         stockSize);
    }

    template<class T>
    static inline T *getNodeCacheMemory(int nodeId, int dateSize, int stockSize, T *cacheMemory) {
        return cacheMemory + nodeId * dateSize * stockSize;
    }


protected:
    void calAlpha(AlphaDB *alphaDataBase, AlphaCache *cache, ThreadPool *threadPool) {

        //int rootId = getSubtreeRootId(rootName);
        for (auto i = 0; i < nodeList_.getSize(); ++i) {
            int nodeId = i;
            AlphaTree *alphaTree = this;
            //cout<<i<<endl;
            cache->nodeRes[i] = threadPool->enqueue([alphaTree, alphaDataBase, nodeId, cache] {
                return alphaTree->cast(alphaDataBase, nodeId, cache);
            }).share();
        }

    }

    int cast(AlphaDB *alphaDataBase, int nodeId, AlphaCache *cache) {
        //cout<<"start "<<nodeList_[nodeId].getName()<<endl;
        int dateSize = GET_ELEMEMT_SIZE(getMaxHistoryDays(), cache->sampleDays);
        //float* curResultCache = getNodeCacheMemory(nodeId, dateSize, cache->stockSize, cache->result);
        //bool* curResultFlag = cache->flagRes[nodeId].get();
        CacheFlag *curDayFlagCache = getNodeFlag(nodeId, dateSize, cache->dayFlag);

        int outMemoryId = 0;
        if (nodeList_[nodeId].getChildNum() == 0) {
            outMemoryId = cache->result->useCacheMemory();
            alphaDataBase->getStock(cache->dayBefore,
                                    getMaxHistoryDays(),
                                    cache->sampleDays,
                                    cache->stockSize,
                                    nodeList_[nodeId].getName(),
                    //((AlphaPar *) nodeList_[nodeId].getElement())->leafDataType,
                                    nodeList_[nodeId].getWatchLeafDataClass(),
                                    (float *) cache->result->getCacheMemory(outMemoryId).cache,
                                    cache->codes);

        } else {
            int childMemoryIds[MAX_CHILD_NUM];
            void *childMemory[MAX_CHILD_NUM];
            for (int i = 0; i < nodeList_[nodeId].getChildNum(); ++i) {
                int childId = nodeList_[nodeId].childIds[i];
                if (nodeList_[childId].isRoot()) {

                    //不能修改子树结果
                    childMemoryIds[i] = cache->result->useCacheMemory();
                    int subTreeMemoryId = cache->nodeRes[childId].get();
                    //cout<<subTreeMemoryId<<" "<<childMemoryIds[i]<<"create new memory\n";
                    memcpy(cache->result->getCacheMemory(childMemoryIds[i]).cache,
                           cache->result->getCacheMemory(subTreeMemoryId).cache,
                           cache->stockSize * dateSize * sizeof(float));
                } else {
                    childMemoryIds[i] = cache->nodeRes[childId].get();
                }
                childMemory[i] = cache->result->getCacheMemory(childMemoryIds[i]).cache;
            }
            //cout<<nodeList_[nodeId].getElement()->getParNum()<<" " << nodeList_[nodeId].getChildNum()<<endl;
            for (int i = 0; i < nodeList_[nodeId].getElement()->getParNum() - nodeList_[nodeId].getChildNum(); ++i) {
                int newMemId = cache->result->useCacheMemory();
                childMemoryIds[i + nodeList_[nodeId].getChildNum()] = newMemId;
                childMemory[i + nodeList_[nodeId].getChildNum()] = cache->result->getCacheMemory(newMemId).cache;
            }

            nodeList_[nodeId].getElement()->cast(childMemory, nodeList_[nodeId].getCoff(coffList_), dateSize,
                                                 cache->stockSize,
                                                 curDayFlagCache);

            //回收内存
            for (int i = 0; i < nodeList_[nodeId].getElement()->getParNum(); ++i) {
                if (i != nodeList_[nodeId].getElement()->getOutParIndex()) {
                    cache->result->releaseCacheMemory(childMemoryIds[i]);
                } else {
                    outMemoryId = childMemoryIds[i];
                }
            }
//                for(size_t i = 0; i < dateSize * cache->stockSize; ++i)
//                    if(isnan(curResultCache[i])){
//                        cout<<"error "<<nodeList_[nodeId].getName()<<endl;
//                        cout<<"left "<<leftRes[i]<<endl;
//                        if(rightRes)
//                            cout<<"right "<<rightRes[i]<<endl;
//                    }

        }
        for (auto i = 0; i < dateSize; ++i) {
            if (curDayFlagCache[i] == CacheFlag::NEED_CAL)
                curDayFlagCache[i] = CacheFlag::HAS_CAL;
        }
        //cout<<nodeList_[nodeId].getName()<<" out "<<outMemoryId<<endl;
        return outMemoryId;
    }

    float getMaxHistoryDays(int nodeId) {
        if (nodeList_[nodeId].getChildNum() == 0)
            return 0;
        float maxDays = 0;
        for (int i = 0; i < nodeList_[nodeId].getChildNum(); ++i) {
            maxDays = max(getMaxHistoryDays(nodeList_[nodeId].childIds[i]), maxDays);
        }
        //cout<<nodeList_[nodeId].getName()<<" "<<nodeList_[nodeId].getNeedBeforeDays(coffList_)<<" "<<maxDays<<endl;
        return nodeList_[nodeId].getNeedBeforeDays(coffList_) + maxDays;
    }


    void flagAllNode(AlphaCache *cache) {

        int dateSize = GET_ELEMEMT_SIZE(getMaxHistoryDays(), cache->sampleDays);
        for (size_t dayIndex = 0; dayIndex < cache->sampleDays; ++dayIndex) {
            for (auto i = 0; i < subtreeList_.getSize(); ++i) {
                int curIndex = dayIndex + getMaxHistoryDays() - 1;
                flagNodeDay(subtreeList_[i].rootId, curIndex, dateSize, cache);
            }
        }

        //for (auto i = 0; i < nodeList_.getSize(); ++i)
         //   cache->nodeFlag[i] = true;
    }

    void flagNodeDay(int nodeId, int dayIndex, int dateSize, AlphaCache *cache) {

        CacheFlag *curFlag = getNodeFlag(nodeId, dateSize, cache->dayFlag);

        //bool* curStockFlag = getNodeCacheMemory(nodeId, dateSize, cache->stockSize, cache->resultFlag) + dayIndex * cache->stockSize;
        if (curFlag[dayIndex] == CacheFlag::NO_FLAG) {
            //填写数据
            curFlag[dayIndex] = CacheFlag::NEED_CAL;

            if (nodeList_[nodeId].getChildNum() > 0) {
                //for(size_t i = 0; i < cache->stockSize; ++i)
                //    curStockFlag[i] = true;
                int dayNum = (int) roundf(nodeList_[nodeId].getNeedBeforeDays(coffList_));
                switch (nodeList_[nodeId].getElement()->getDateRange()) {
                    case DateRange::CUR_DAY:

                        flagAllChild(nodeId, dayIndex, dateSize, cache);

                        //flagStock(getLeftFlag(nodeId, dayIndex, dateSize, cache), getRightFlag(nodeId, dayIndex, dateSize, cache), curStockFlag, cache->stockSize);
                        break;
                    case DateRange::BEFORE_DAY:
                        flagAllChild(nodeId, dayIndex - dayNum, dateSize, cache);
                        //flagStock(getLeftFlag(nodeId, dayIndex - dayNum, dateSize, cache), getRightFlag(nodeId, dayIndex - dayNum, dateSize, cache), curStockFlag, cache->stockSize);
                        break;
                    case DateRange::CUR_AND_BEFORE_DAY:
                        flagAllChild(nodeId, dayIndex, dateSize, cache);
                        flagAllChild(nodeId, dayIndex - dayNum, dateSize, cache);
                        //flagStock(getLeftFlag(nodeId, dayIndex, dateSize, cache), getRightFlag(nodeId, dayIndex, dateSize, cache), curStockFlag, cache->stockSize);
                        //flagStock(getLeftFlag(nodeId, dayIndex - dayNum, dateSize, cache), getRightFlag(nodeId, dayIndex - dayNum, dateSize, cache), curStockFlag, cache->stockSize);
                        break;
                    case DateRange::ALL_DAY:
                        for (auto i = 0; i <= dayNum; ++i) {
                            flagAllChild(nodeId, dayIndex - i, dateSize, cache);
                            //flagStock(getLeftFlag(nodeId, dayIndex - dayNum, dateSize, cache), getRightFlag(nodeId, dayIndex - i, dateSize, cache), curStockFlag, cache->stockSize);
                        }
                        break;
                }

            }
//                else{
//                    alphaDataBase->getFlag(dayIndex, cache->dayBefore, getMaxHistoryDays(), cache->sampleDays, cache->stockSize, curStockFlag, cache->codes);
//                }
        }

    }

    /*void flagNodeStock(int nodeId, int dayIndex, int stockIndex, AlphaDB *alphaDataBase, size_t dayBefore,
                       size_t sampleSize, bool *flagCache,
                       const char *curCode, size_t stockSize, bool isCalAllNode = false){
        int dateSize = GET_ELEMEMT_SIZE(getMaxHistoryDays(), sampleSize);
        CHECK(dayIndex >= 0 && dayIndex < dateSize, "标记错误");
        bool * curFlagCache = getNodeCacheMemory(nodeId, dateSize, stockSize, flagCache);
        int deltaSpace = dateSize + dayBefore - dayIndex;
        //只需要标记没有标记过的
        int curIndex = dayIndex * stockSize + stockIndex;
        if(curFlagCache[curIndex] == false){
            if(nodeList_[nodeId].getChildNum() == 0){
                bool* alphaDataBase->getFlag()
                Stock* stock = alphaDataBase->getStock(curCode, nodeList_[nodeId].getWatchLeafDataClass());
                if(stock == nullptr || stock->size < deltaSpace || stock->volume[stock->size - deltaSpace] <= 0){
                    curFlagCache[curIndex] = CacheFlag::CAN_NOT_FLAG;
                    //cout<<stock->code<<" "<<curCode<<" "<<nodeList_[nodeId].getWatchLeafDataClass()<<" "<<stock->size<<"-"<<deltaSpace<<" "<<stockIndex<<" stock err"<<endl;
                } else{
                    curFlagCache[curIndex] = CacheFlag::NEED_CAL;
                }
            } else{
                //先标记孩子
                int dayNum = (int)roundf(nodeList_[nodeId].getNeedBeforeDays(coffList_));
                switch(nodeList_[nodeId].getElement()->getDateRange()){
                    case DateRange::CUR_DAY:
                        flagAllChild(nodeId, dayIndex, stockIndex, alphaDataBase, dayBefore, sampleSize, flagCache, curCode, stockSize, isCalAllNode);
                        break;
                    case DateRange::BEFORE_DAY:
                        flagAllChild(nodeId,dayIndex - dayNum, stockIndex, alphaDataBase, dayBefore, sampleSize, flagCache, curCode, stockSize, isCalAllNode);
                        break;
                    case DateRange::CUR_AND_BEFORE_DAY:
                        flagAllChild(nodeId, dayIndex, stockIndex, alphaDataBase, dayBefore, sampleSize, flagCache, curCode, stockSize, isCalAllNode);
                        flagAllChild(nodeId,dayIndex - dayNum, stockIndex, alphaDataBase, dayBefore, sampleSize, flagCache, curCode, stockSize, isCalAllNode);
                        break;
                    case DateRange::ALL_DAY:
                        for(auto i = 0; i <= dayNum; ++i)
                            flagAllChild(nodeId,dayIndex - i, stockIndex, alphaDataBase, dayBefore, sampleSize, flagCache, curCode, stockSize, isCalAllNode);
                        break;
                }
                if(isCalAllNode){
                    for(int i = getMaxHistoryDays() - 1; i < getMaxHistoryDays() + sampleSize - 1; ++i){
                        flagAllChild(nodeId,i, stockIndex, alphaDataBase, dayBefore, sampleSize, flagCache, curCode, stockSize, isCalAllNode);
                    }
                }
                //再标记自己
                CacheFlag * leftFlagCache = getNodeCacheMemory(nodeList_[nodeId].leftId, dateSize, stockSize, flagCache);
                CacheFlag * rightFlagCache = nodeList_[nodeId].getChildNum() > 1 ? getNodeCacheMemory(nodeList_[nodeId].rightId, dateSize, stockSize, flagCache) : nullptr;
                switch(nodeList_[nodeId].getElement()->getDateRange()){
                    case DateRange::CUR_DAY:
                        curFlagCache[curIndex] = getFlag(leftFlagCache, rightFlagCache, curIndex);
                        break;
                    case DateRange::BEFORE_DAY:
                        curFlagCache[curIndex] = getFlag(leftFlagCache, rightFlagCache, curIndex - dayNum * stockSize);
                        break;
                    case DateRange::CUR_AND_BEFORE_DAY:
                        curFlagCache[curIndex] = CacheFlag::NEED_CAL;
                        if(getFlag(leftFlagCache, rightFlagCache, curIndex) == CacheFlag::CAN_NOT_FLAG || getFlag(leftFlagCache, rightFlagCache, curIndex - dayNum * stockSize) == CacheFlag::CAN_NOT_FLAG)
                            curFlagCache[curIndex] = CacheFlag::CAN_NOT_FLAG;
                        break;
                    case DateRange::ALL_DAY:
                        curFlagCache[curIndex] = CacheFlag::NEED_CAL;
                        for(auto i = 0; i <= dayNum; ++i){
                            if(getFlag(leftFlagCache, rightFlagCache, curIndex - i * stockSize) == CacheFlag::CAN_NOT_FLAG){
                                curFlagCache[curIndex] = CacheFlag::CAN_NOT_FLAG;
                                return;
                            }
                        }
                        break;
                }
            }
        }
    }*/

    int maxHistoryDay_;

private:
    inline static bool *
    flag(DateRange dataRange, const bool *pleft, const bool *pright, size_t day, size_t historySize, size_t stockSize,
         CacheFlag *pflag, bool *pout) {

        if (day > 0) {
            memset(pout, 0, day * stockSize * sizeof(bool));
        }
        for (size_t j = 0; j < stockSize; ++j) {
            for (size_t i = day; i < historySize; ++i) {
                if (pflag[i] == CacheFlag::NEED_CAL) {
                    size_t watchIndex = 0;
                    size_t curIndex = i * stockSize + j;
                    switch (dataRange) {
                        case DateRange::CUR_DAY:
                            watchIndex = i * stockSize + j;
                            pout[curIndex] = (pleft[watchIndex] && (pright == nullptr || pright[watchIndex]));
                            break;
                        case DateRange::BEFORE_DAY:
                            watchIndex = (i - day) * stockSize + j;
                            pout[curIndex] = (pleft[watchIndex] && (pright == nullptr || pright[watchIndex]));
                            break;
                        case DateRange::CUR_AND_BEFORE_DAY:
                            watchIndex = i * stockSize + j;
                            pout[curIndex] = (pleft[watchIndex] && (pright == nullptr || pright[watchIndex]));
                            if (pout[curIndex]) {
                                watchIndex = (i - day) * stockSize + j;
                                pout[curIndex] = (pleft[watchIndex] && (pright == nullptr || pright[watchIndex]));
                            }
                            break;
                        case DateRange::ALL_DAY:
                            if (i > day && pout[curIndex - stockSize]) {
                                watchIndex = i * stockSize + j;
                                pout[curIndex] = (pleft[watchIndex] && (pright == nullptr || pright[watchIndex]));
                            } else {
                                watchIndex = (i - day - 1) * stockSize + j;
                                if (i > day && (pleft[watchIndex] && (pright == nullptr || pright[watchIndex]))) {
                                    pout[curIndex] = false;
                                } else {
                                    for (size_t k = 0; k <= day; ++k) {
                                        watchIndex = (i - k) * stockSize + j;
                                        pout[curIndex] = (pleft[watchIndex] &&
                                                          (pright == nullptr || pright[watchIndex]));
                                        if (pout[curIndex] == false)
                                            break;
                                    }
                                }

                            }
                            break;
                    }

                } else {
                    memset(pout + i * stockSize, 0, stockSize * sizeof(bool));
                }
            }
        }
        return pout;
    }

    inline void flagAllChild(int nodeId, int dayIndex, int dateSize, AlphaCache *cache) {

        for (int i = 0; i < nodeList_[nodeId].getChildNum(); ++i)
            flagNodeDay(nodeList_[nodeId].childIds[i], dayIndex, dateSize, cache);
        //flagNodeDay(nodeList_[nodeId].leftId, dayIndex, dateSize, cache);
        //if(nodeList_[nodeId].getChildNum() > 1)
        //    flagNodeDay(nodeList_[nodeId].rightId, dayIndex, dateSize, cache);
    }

//        static inline void flagStock(const bool* leftFlag, const bool* rightFlag, bool* resultflag, size_t stockSize){
//            for(size_t i = 0; i < stockSize; ++i){
//                if(resultflag[i]){
//                    if(!leftFlag[i])
//                        resultflag[i] = false;
//                    else if(rightFlag && !rightFlag[i])
//                        resultflag[i] = false;
//                }
//            }
//        }

    static inline CacheFlag *getNodeFlag(int nodeId, int dateSize, CacheFlag *flagCache) {
        return flagCache + nodeId * dateSize;
    }

//        inline bool* getLeftFlag(int nodeId, int dayIndex, int dateSize, AlphaCache* cache){
//            return nodeList_[nodeId].getChildNum() > 0 ? getNodeCacheMemory(nodeList_[nodeId].leftId, dateSize, cache->stockSize, cache->resultFlag) + dayIndex * cache->stockSize: nullptr;
//        }
//        inline bool* getRightFlag(int nodeId, int dayIndex, int dateSize, AlphaCache* cache){
//            return nodeList_[nodeId].getChildNum() > 1 ? getNodeCacheMemory(nodeList_[nodeId].rightId, dateSize, cache->stockSize, cache->resultFlag) + dayIndex * cache->stockSize: nullptr;
//        }
};


#endif //ALPHATREE_ALPHATREE_H

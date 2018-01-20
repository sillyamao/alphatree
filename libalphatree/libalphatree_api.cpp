//
// Created by yanyu on 2017/7/20.
//

#include "alphaforest.h"
#include <iostream>

using namespace std;

#define MAX_TREE_SIZE 32768
#define MAX_SUB_TREE_SIZE 16
#define MAX_NODE_SIZE 64
#define MAX_STOCK_SIZE 3600
#define MAX_HISTORY_DAYS 250
#define MAX_SAMPLE_DAYS 2500
#define MAX_FUTURE_DAYS 80
#define CODE_LEN 64


extern "C"
{
void initializeAlphaforest(int cacheSize) {
    AlphaForest::initialize(cacheSize);
    //FilterMachine::initialize(cacheSize);
}

void releaseAlphaforest() {
    AlphaForest::release();
}

int createAlphatree() {
    return AlphaForest::getAlphaforest()->useAlphaTree();
}

void releaseAlphatree(int alphatreeId) {
    AlphaForest::getAlphaforest()->releaseAlphaTree(alphatreeId);
}

int useCache() {
    return AlphaForest::getAlphaforest()->useCache();
}

void releaseCache(int cacheId) {
    AlphaForest::getAlphaforest()->releaseCache(cacheId);
}

int encodeAlphatree(int alphatreeId, const char *rootName, char *out) {
    const char *res = AlphaForest::getAlphaforest()->encodeAlphaTree(alphatreeId, rootName, out);
    return strlen(res);
}

void decodeAlphatree(int alphaTreeId, const char *rootName, const char *line) {
    AlphaForest::getAlphaforest()->decode(alphaTreeId, rootName, line);
}

/*
int learnFilterForest(int alphatreeId, int cacheId, const char *features, int featureSize, int treeSize,
                       int iteratorNum, float gamma, float lambda, int maxDepth,
                       int maxLeafSize, float adjWeightRule,
                       int maxBarSize, float subsample,
                       float colsampleBytree, const char *targetValue) {
    AlphaCache *cache = AlphaForest::getAlphaforest()->getCache(cacheId);
    const float *target = AlphaForest::getAlphaforest()->getAlpha(alphatreeId, targetValue, cacheId);
    const int* sign = AlphaForest::getAlphaforest()->getSign(alphatreeId, cacheId);

    //计算最大取样数据量
    size_t sampleSize = 0;
    for (size_t i = 0; i < cache->stockSize; ++i) {
        for (size_t j = 0; j < cache->sampleDays; ++j) {
            size_t index = j * cache->stockSize + i;
            if (sign[index] > 0) {
                ++sampleSize;
            }
        }
    }

    int filterCacheId = FilterMachine::getFM()->useCache();
    FilterCache *filterCache = FilterMachine::getFM()->getCache(filterCacheId);
    filterCache->initialize(sampleSize, featureSize, maxLeafSize);

    //填写训练参数
    filterCache->treeSize = treeSize;
    filterCache->iteratorNum = iteratorNum;
    filterCache->maxDepth = maxDepth;
    filterCache->maxLeafSize = maxLeafSize;
    filterCache->gamma = gamma;
    filterCache->lambda = lambda;
    filterCache->adjWeightRule = adjWeightRule;
    filterCache->maxBarSize = maxBarSize;
    filterCache->subsample = subsample;
    filterCache->colsampleBytree = colsampleBytree;

    //填写特征
    const char *curFeature = features;
    AlphaForest::getAlphaforest()->getAlphaDataBase()->getFeature(cache->dayBefore, cache->sampleDays, cache->stockSize, cache->codes, sign,
                              filterCache->feature, sampleSize, features, featureSize);

    //填写特征名字
    for (size_t fId = 0; fId < featureSize; ++fId) {
        strcpy(filterCache->featureName + fId * MAX_FEATURE_NAME_LEN, curFeature);
        curFeature += strlen(curFeature) + 1;
    }

    //填写目标
    size_t sampleIndex = 0;
    for (size_t i = 0; i < cache->stockSize; ++i) {
        for (size_t j = 0; j < cache->sampleDays; ++j) {
            size_t index = j * cache->stockSize + i;
            if (sign[index] > 0) {
                filterCache->target[sampleIndex++] = target[index];
            }
        }
    }

    int forestId = FilterMachine::getFM()->useFilterForest();

    FilterMachine::getFM()->train(filterCacheId, forestId);

    FilterMachine::getFM()->releaseCache(filterCacheId);
    return forestId;
}*/

void loadDataBase(const char *path) {
    AlphaForest::getAlphaforest()->getAlphaDataBase()->loadDataBase(path);
}

//    void loadStockMeta(const char* codes, int* marketIndex, int* industryIndex, int* conceptIndex, int size, int days){
//        AlphaForest::getAlphaforest()->getAlphaDataBase()->loadStockMeta(codes, marketIndex, industryIndex, conceptIndex, size, days);
//    }
//
//    void loadDataElement(const char* elementName, const float* data, int needDays){
//        AlphaForest::getAlphaforest()->getAlphaDataBase()->loadDataElement(elementName, data, needDays);
//    }

int getStockCodes(char *codes) {
    return AlphaForest::getAlphaforest()->getAlphaDataBase()->getStockCodes(codes);
}

int getMarketCodes(const char *marketName, char *codes) {
    return AlphaForest::getAlphaforest()->getAlphaDataBase()->getMarketCodes(marketName, codes);
}

int getIndustryCodes(const char *industryName, char *codes) {
    return AlphaForest::getAlphaforest()->getAlphaDataBase()->getIndustryCodes(industryName, codes);
}

int getMaxHistoryDays(int alphaTreeId) { return AlphaForest::getAlphaforest()->getMaxHistoryDays(alphaTreeId); }


void calAlpha(int alphaTreeId, int cacheId, int dayBefore, int sampleSize, const char *codes, int stockSize) {
    AlphaForest::getAlphaforest()->calAlpha(alphaTreeId, cacheId, dayBefore, sampleSize, codes, stockSize);
}

void cacheAlpha(int alphaTreeId, int cacheId, bool isToFile) {
    AlphaForest::getAlphaforest()->cacheAlpha(alphaTreeId, cacheId, isToFile);
}

float optimizeAlpha(int alphaTreeId, int cacheId, const char *rootName, int dayBefore, int sampleSize, const char *codes, size_t stockSize, float exploteRatio, int errTryTime){
    return AlphaForest::getAlphaforest()->optimizeAlpha(alphaTreeId, cacheId, rootName, dayBefore, sampleSize, codes, stockSize, exploteRatio, errTryTime);
}

int getRootAlpha(int alphaTreeId, const char *rootName, int cacheId, float *alpha) {
    const float *res = AlphaForest::getAlphaforest()->getAlpha(alphaTreeId, rootName, cacheId);
    auto *cache = AlphaForest::getAlphaforest()->getCache(cacheId);
    int dataSize = cache->sampleDays * cache->stockSize;
    memcpy(alpha, res, dataSize * sizeof(float));
    return dataSize;
}

void getRootProcess(int alphatreeId, const char *rootName, int cacheId, char* process){
    const char* rootProcess = AlphaForest::getAlphaforest()->getProcess(alphatreeId, rootName, cacheId);
    strcpy(process,rootProcess);
}

/*
int getNodeAlpha(int alphaTreeId, int nodeId, int cacheId, float *alpha) {
    const float *res = AlphaForest::getAlphaforest()->getAlpha(alphaTreeId, nodeId, cacheId);
    auto *cache = AlphaForest::getAlphaforest()->getCache(cacheId);
    int dataSize = cache->sampleDays * cache->stockSize;
    memcpy(alpha, res, dataSize * sizeof(float));
    return dataSize;
}*/


//总结子公式
int summarySubAlphaTree(const int *alphatreeIds, int len, int minDepth, char *subAlphatreeStr) {
    return AlphaForest::getAlphaforest()->summarySubAlphaTree(alphatreeIds, len, minDepth, subAlphatreeStr);
}

}

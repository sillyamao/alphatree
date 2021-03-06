//
// Created by yanyu on 2017/10/12.
//

#ifndef ALPHATREE_BASEALPHATREE_H
#define ALPHATREE_BASEALPHATREE_H

#include "alphaatom.h"
#include "converter.h"
#include "alphadb.h"

const size_t MAX_NODE_STR_LEN = 512;
const size_t MAX_SUB_ALPHATREE_STR_NUM = 512;
//const size_t MAX_DECODE_RANGE_LEN = 10;
const size_t MAX_OPT_STR_LEN = 64;

const size_t MAX_LEAF_DATA_STR_LEN = 64;
const size_t MAX_NODE_NAME_LEN = 64;

const size_t MAX_SUB_TREE_BLOCK = 16;
const size_t MAX_NODE_BLOCK = 64;
const size_t MAX_CHILD_NUM = 16;

#define STOCK_SIZE 6000
#define HISTORY_DAYS 512
#define SAMPLE_DAYS 2500

struct AlphaCoff{
    float coffValue;
    int srcNodeIndex;
};

class AlphaNode{
public:
    //是否是子树
    bool isRoot(){ return preId == -1;}
    //得到名字
    const char* getName(){ return element_ ?  element_->getName() : dataName_;}
    //得到系数
    float getCoff(DArray<AlphaCoff, MAX_NODE_BLOCK>& coffList){
        if(getCoffUnit() != CoffUnit::COFF_NONE
           && getCoffUnit() != CoffUnit::COFF_INDCLASS
           && getCoffUnit()!=CoffUnit::COFF_CONST)
            return coffList[externalCoffIndex_].coffValue;
        return startCoff_;
    }

    float getStartCoff(){ return startCoff_;}

    bool isEmpty(){
        return this->dataName_[0] == 0 && this->element_ == nullptr;
    }

    //得到分类
    const char* getWatchLeafDataClass(){return dataClass_[0] == 0 ? nullptr : dataClass_;}
    //得到系数
    int getCoffStr(char* coffStr, DArray<AlphaCoff, MAX_NODE_BLOCK>& coffList){
        //写系数
        switch(getCoffUnit()){
            case CoffUnit::COFF_NONE:
                coffStr[0] = 0;
                return 0;
            case CoffUnit::COFF_DAY:
            case CoffUnit::COFF_VAR:
            case CoffUnit::COFF_CONST:
            {
                sprintf(coffStr,"%.8f", getCoff(coffList));
                int curIndex = strlen(coffStr);
                while(coffStr[curIndex-1] == '0' || coffStr[curIndex-1] == '.') {
                    --curIndex;
                    if(coffStr[curIndex] == '.'){
                        coffStr[curIndex] = 0;
                        break;
                    }
                }
                coffStr[curIndex] = 0;
                return curIndex;
            }
            case CoffUnit::COFF_INDCLASS:
                strcpy(coffStr, getWatchLeafDataClass());
                return strlen(coffStr);
        }
        return 0;
    }


    void setup(IAlphaElement* element, int elementId, DArray<AlphaCoff, MAX_NODE_BLOCK>& coffList, float coff = 0, const char* watchLeafDataClass = nullptr){
        //CHECK(element != nullptr, "err");
        externalCoffIndex_ = -1;
        setup(nullptr, watchLeafDataClass);
        element_ = element;
        if(element != nullptr
           && element->getCoffUnit() != CoffUnit::COFF_INDCLASS
           && element->getCoffUnit() != CoffUnit::COFF_NONE
           && element->getCoffUnit() != CoffUnit::COFF_CONST){
            externalCoffIndex_ = coffList.getSize();
            coffList[externalCoffIndex_].coffValue = coff;
            coffList[externalCoffIndex_].srcNodeIndex = elementId;

        }
        startCoff_ = coff;
        preId = -1;
        childNum = 0;
    }

    void setup(const char* name = nullptr, const char* watchLeafDataClass = nullptr){
        if(watchLeafDataClass)
            strcpy(dataClass_, watchLeafDataClass);
        else
            dataClass_[0] = 0;
        if(name)
            strcpy(dataName_, name);
        else
            dataName_[0] = 0;
        element_ = nullptr;
        childNum = 0;
    }

    int getChildNum(){return childNum;}
    void addChild(int id){childIds[childNum++] = id;}

    CoffUnit getCoffUnit(){return element_->getCoffUnit();}

    float getNeedBeforeDays(DArray<AlphaCoff, MAX_NODE_BLOCK>& coffList){
        if(getCoffUnit() == CoffUnit::COFF_DAY){
            if(isnan(coffList[externalCoffIndex_].coffValue))
            {
                cout<<"error "<<externalCoffIndex_<<endl;
            }
            return coffList[externalCoffIndex_].coffValue;
        }

        return 0;
        //float day = coffList[externalCoffIndex_];
        //return fmaxf(day, element_->getMinHistoryDays());
    }

    IAlphaElement* getElement(){ return element_;}

    int getExternalCoffId(){ return externalCoffIndex_;};
    int childIds[MAX_CHILD_NUM];
    int childNum = {0};
    int preId = {-1};
protected:
    int externalCoffIndex_ = {-1};
    char dataClass_[MAX_LEAF_DATA_STR_LEN] = {0};
    char dataName_[MAX_LEAF_DATA_STR_LEN] = {0};
    IAlphaElement* element_ = {nullptr};
    float startCoff_ = {0};
};


//注意,规定子树中不能包含工业数据
class SubTreeDes{
public:
    char name[MAX_NODE_NAME_LEN];
    int rootId;

};

//仅仅负责树的构造,不负责运算
class BaseAlphaTree{
public:

    BaseAlphaTree(){
        clean();
    }

    virtual ~BaseAlphaTree(){
        coffList_.clear();
        nodeList_.clear();
        subtreeList_.clear();
        //processList_.clear();
    }

    virtual void clean(){
        coffList_.resize(0);
        nodeList_.resize(0);
        subtreeList_.resize(0);
        //processList_.resize(0);
    }

    int getSubtreeSize(){ return subtreeList_.getSize();}
    const char* getSubtreeName(int index){ return subtreeList_[index].name;}

    virtual void setCoff(int index, float coff){
        coffList_[index].coffValue = coff;
    }

    float getCoff(int index){
        return coffList_[index].coffValue;
    }

    float getMaxCoff(int index){
        return nodeList_[coffList_[index].srcNodeIndex].getElement()->getMaxCoff();
    }

    float getMinCoff(int index){
        return nodeList_[coffList_[index].srcNodeIndex].getElement()->getMinCoff();
    }

    CoffUnit getCoffUnit(int index){
        return nodeList_[coffList_[index].srcNodeIndex].getElement()->getCoffUnit();
    }

    int getCoffSize(){
        return coffList_.getSize();
    }


    //必须先解码子树再解码主树
    void decode(const char* rootName, const char* line, HashMap<IAlphaElement*>& alphaElementMap){
//        cout<<line<<"---"<<endl;
        int outCache[(MAX_CHILD_NUM+1) * 2];
        char optCache[MAX_OPT_STR_LEN];
        char normalizeLine[MAX_NODE_STR_LEN];
        converter_.operator2function(line, normalizeLine);
        //cout<<normalizeLine<<endl;
        int subtreeLen = subtreeList_.getSize();
        strcpy(subtreeList_[subtreeLen].name, rootName);
        int rootId = decode(normalizeLine, alphaElementMap, 0, strlen(normalizeLine)-1, outCache, optCache);
        subtreeList_[subtreeLen].rootId = rootId;
        //subtreeList_[subtreeLen].isLocal = isLocal;
        //恢复被写坏的preId
        for(auto i = 0; i < subtreeLen; ++i)
            nodeList_[subtreeList_[i].rootId].preId = -1;
        //cout<<"finish decode\n";
    }


    //编码成字符串
    const char* encode(const char* rootName, char* pout){
        int curIndex = 0;
        char normalizeLine[MAX_NODE_STR_LEN];
        encode(normalizeLine, getSubtreeRootId(rootName), curIndex, getSubtreeIndex(rootName));
        normalizeLine[curIndex] = 0;
        converter_.function2operator(normalizeLine, pout);
        return pout;
    }


//    int searchPublicSubstring(const char* rootName, const char* line, char* pout, int minTime = 1, int minDepth = 3){
//        int curIndex = 0;
//        //缓存编码出来的字符串,*2是因为字符串包括编码成符号的
//        char normalizeLine[MAX_NODE_STR_LEN * 2];
//        return searchPublicSubstring(getSubtreeRootId(rootName), getSubtreeIndex(rootName), line, normalizeLine, curIndex, pout, minTime, minDepth);
//    }
protected:
    int decode(const char* line, HashMap<IAlphaElement*>& alphaElementMap, int l, int r, int* outCache, char* optCache, const char* parDataClass = nullptr){
        if(strlen(line) == 0)
            return createNode(nullptr,nullptr);//创建一个空节点，可以用来作为变量
        converter_.decode(line, l, r, outCache);
        const char* opt = converter_.readOpt(line, outCache, optCache);
        //cout<<"Start "<<opt<<endl;
        //读出系数
        float coff = 0;

        //读出操作符
        IAlphaElement* alphaElement  = nullptr;
        //创建节点
        int nodeId = NONE;
        //读出左右孩子
        //int ll = outCache[2], lr = outCache[3], rl = outCache[4], rr = outCache[5];

        //int leftId = -1, rightId = -1;

        char curDataClass[MAX_LEAF_DATA_STR_LEN];
        if(parDataClass != nullptr)
            strcpy(curDataClass, parDataClass);
        char* dataClass = (parDataClass == nullptr) ? nullptr : curDataClass;

        //特殊处理一些操作符的系数
        if(converter_.isSymbolFun(opt) || converter_.isCmpFun(opt)){
            //如果第一个孩子是数字,变成opt_from
            if(converter_.getOptSize(outCache, 1) < MAX_OPT_STR_LEN && converter_.isNum(converter_.readOpt(line, outCache, optCache,1))){
                coff = atof(optCache);

                //重新得到刚才的操作数
                opt = converter_.readOpt(line, outCache, optCache);
                //在操作符后面拼接上_from
                char* p = optCache + strlen(opt);
                strcpy(p, "_from");
                alphaElement = alphaElementMap[optCache];

                int childId = decode(line, alphaElementMap, outCache[4], outCache[5], outCache, optCache, dataClass);
                nodeId = createNode(alphaElement, coff, dataClass);
                //字符串的第一部分是系数,第二部分才是子孩子,添加第二部分
                addChild(nodeId, childId);
                //将这种系数在左边的特殊情况直接返回
                return nodeId;
            } else if(converter_.getOptSize(outCache, 2) < MAX_OPT_STR_LEN && converter_.isNum(converter_.readOpt(line, outCache, optCache, 2))){
                //重新得到刚才的操作数
                opt = converter_.readOpt(line, outCache, optCache);
                //在操作符后面拼接上_from
                char* p = optCache + strlen(opt);
                strcpy(p, "_to");
            } else {
                //重新得到刚才的操作数
                opt = converter_.readOpt(line, outCache, optCache);
            }
        }

        //特殊处理子树
        for(auto i = 0; i < subtreeList_.getSize(); i++){
            if(strcmp(opt, subtreeList_[i].name) == 0){
                return subtreeList_[i].rootId;
            }
        }

        //特殊处理叶节点
        auto iter = *alphaElementMap.find(opt);
        if(iter == nullptr){
            nodeId = createNode(opt, dataClass);
            return nodeId;
        }

        alphaElement = alphaElementMap[opt];
        int childNum = converter_.getElementNum(outCache) - 1;

        if(alphaElement->getCoffUnit() != CoffUnit::COFF_NONE){
            //最后一个孩子是系数;
            converter_.readOpt(line, outCache, optCache, childNum);
            --childNum;

            switch (alphaElement->getCoffUnit()){
                case CoffUnit::COFF_DAY:
                case CoffUnit::COFF_VAR:
                case CoffUnit::COFF_CONST:
                    coff = atof(optCache);
                    break;
                case CoffUnit::COFF_INDCLASS:
                    strcpy(curDataClass, optCache);
                    dataClass = curDataClass;
                    break;
                case CoffUnit::COFF_FORBIT_INDCLASS:
                    dataClass = nullptr;
                    break;
                default:
                    cout<<"coff error\n";
                    throw "coff error!";
            }
        }

        //保证自底向上的方式创建节点,根节点一定排在数组最后面
        int childL[MAX_CHILD_NUM];
        int childR[MAX_CHILD_NUM];
        for(int i = 0; i < childNum; ++i){
            childL[i] = outCache[2 + i * 2];
            childR[i] = outCache[3 + i * 2];
        }
        int chidIds[MAX_CHILD_NUM];
        for(int i = 0; i < childNum; ++i){
            chidIds[i] = decode(line, alphaElementMap, childL[i], childR[i], outCache, optCache, dataClass);
        }

        nodeId = createNode(alphaElement, coff, dataClass);
        for(int i = 0; i < childNum; ++i)
            addChild(nodeId, chidIds[i]);
        //cout<<"finish "<<opt<<endl;
        return nodeId;
    }

    void encode(char* pout, int nodeId, int& curIndex, int subtreeSize){
        const char* name = getSubtreeRootName(nodeId, subtreeSize);
        //编码子树
        if(name != nullptr){
            strcpy(pout + curIndex, name);
            curIndex += strlen(name);
            return;
        }
        AlphaNode* node = &nodeList_[nodeId];

        //这个节点如果只作为变量，什么不用做。
        if(node->isEmpty())
            return;

        name = node->getName();
        int nameLen = strlen(name);

        //特殊处理系数在左边的情况
        if(nameLen > 5 && strcmp(name + (nameLen-5),"_from") == 0){
            //先写名字
            memcpy(pout + curIndex, node->getName(), (nameLen - 5) * sizeof(char));
            curIndex += (nameLen - 5);
            pout[curIndex++] = '(';

            //写系数
            int coffLen = node->getCoffStr(pout + curIndex, coffList_);
            curIndex += coffLen;

            //写左孩子
            pout[curIndex++] = ',';
            pout[curIndex++] = ' ';
            encode(pout, node->childIds[0], curIndex, subtreeSize);

            pout[curIndex++] = ')';
        } else {

            //特殊处理一些符号
            if(nameLen > 3 && strcmp(name + (nameLen-3),"_to") == 0)
                nameLen -= 3;

            //先写名字
            memcpy(pout + curIndex, node->getName(), nameLen * sizeof(char));
            curIndex += nameLen;

            if(node->getChildNum() > 0) {
                pout[curIndex++] = '(';
                encode(pout, node->childIds[0], curIndex, subtreeSize);


                for(int i = 1; i < node->getChildNum(); ++i){
                    pout[curIndex++] = ',';
                    pout[curIndex++] = ' ';
                    encode(pout, node->childIds[i], curIndex, subtreeSize);
                }

                //写系数
                if(node->getCoffUnit() != CoffUnit::COFF_NONE){
                    pout[curIndex++] = ',';
                    pout[curIndex++] = ' ';
                    int coffLen = node->getCoffStr(pout + curIndex, coffList_);
                    curIndex += coffLen;
                }

                pout[curIndex++] = ')';
            }
        }
    }


    const char* getSubtreeRootName(int nodeId, int subtreeSize = -1){
        if(subtreeSize == -1)
            subtreeSize = subtreeList_.getSize();
        if(nodeList_[nodeId].isRoot()){
            for(auto i = 0; i < subtreeSize; ++i){
                if(subtreeList_[i].rootId == nodeId){
                    return subtreeList_[i].name;
                }
            }
        }
        return nullptr;
    }

    int getSubtreeRootId(const char* rootName){
        for(auto i = 0; i < subtreeList_.getSize(); ++i)
            if(strcmp(subtreeList_[i].name, rootName) == 0)
                return subtreeList_[i].rootId;
        return -1;
    }

    int getSubtreeIndex(const char* rootName){
        for(auto i = 0; i < subtreeList_.getSize(); ++i)
            if(strcmp(subtreeList_[i].name, rootName) == 0)
                return i;
        return -1;
    }


    inline void addChild(int nodeId, int childId){
        nodeList_[nodeId].addChild(childId);
        nodeList_[childId].preId = nodeId;
    }

    inline int createNode(IAlphaElement* element, float coff = 0, const char* watchLeafDataClass = nullptr){
        int nodeLen = nodeList_.getSize();
        nodeList_[nodeLen].setup(element, nodeLen, coffList_, coff, watchLeafDataClass);
        return nodeLen;
    }

    inline int createNode(const char* name, const char* watchLeafDataClass = nullptr){
        int nodeLen = nodeList_.getSize();
        nodeList_[nodeLen].setup(name, watchLeafDataClass);
        return nodeLen;
    }


    //搜索某个节点下所有子串是否在line中出现minTime以上(包括minTime),限制节点的最小深度minDepth
//    int searchPublicSubstring(int nodeId, int subtreeSize, const char* line, char* encodeCache, int& curIndex, char* pout, int minTime = 1, int minDepth = 3){
//        if(getDepth(nodeId, subtreeSize) < minDepth)
//            return 0;
//        int encodeIndex = 0;
//        encode(encodeCache, nodeId, encodeIndex, subtreeSize);
//        encodeCache[encodeIndex] = 0;
//        char* realSubstring = encodeCache + (encodeIndex+1);
//        converter_.function2operator(encodeCache, realSubstring);
//
//        int count = 0;
//        const char* subline = strstr(line, realSubstring);
//        while (subline){
//            ++count;
//            subline += 1;
//            subline = strstr(subline, realSubstring);
//        }
//
//        if(count >= minTime){
//            strcpy(pout + curIndex, realSubstring);
//            curIndex += (strlen(realSubstring) + 1);
//            return 1;
//        }
//
//        count = 0;
//        for(int i = 0;i < nodeList_[nodeId].getChildNum(); ++i)
//            count += searchPublicSubstring(nodeList_[nodeId].childIds[i], subtreeSize, line, encodeCache, curIndex, pout, minTime, minDepth);
//        return count;
//    }

    int getDepth(int nodeId, int subtreeSize = -1){
        if(nodeList_[nodeId].getChildNum() == 0 || getSubtreeRootName(nodeId, subtreeSize) != nullptr)
            return 1;

        int myDepth = 1;

        int childDepth = 0;
        for(int i = 0; i < nodeList_[nodeId].getChildNum(); ++i){
            childDepth = std::max(childDepth,getDepth(nodeList_[nodeId].childIds[i], false));
        }
        return childDepth + myDepth;
    }
protected:
    //某个节点的参数可以从coffList_中读取,方便最优化

    DArray<AlphaCoff, MAX_NODE_BLOCK> coffList_;
    DArray<AlphaNode, MAX_NODE_BLOCK> nodeList_;
    DArray<SubTreeDes, MAX_SUB_TREE_BLOCK> subtreeList_;
    //DArray<AlphaProcessNode, MAX_PROCESS_BLOCK> processList_;
    static AlphaTreeConverter converter_;
};

AlphaTreeConverter BaseAlphaTree::converter_ = AlphaTreeConverter();

#endif //ALPHATREE_BASEALPHATREE_H

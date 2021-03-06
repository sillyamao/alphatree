//
// Created by yanyu on 2017/11/22.
//

#ifndef ALPHATREE_BASEFILTERTREE_H
#define ALPHATREE_BASEFILTERTREE_H

#include "hashmap.h"

namespace fb {
#define CHECK(isSuccess, err) if(!(isSuccess)) throw err
    const size_t MAX_FILTER_NODE_NAME_LEN = 64;
    const size_t MAX_FILTER_NODE_BLOCK = 64;
    const size_t MAX_FILTER_NODE_STR_LEN = 1024;

    float atof(const char *str)
    {
        float s=0.0;

        float d=10.0;
        int jishu=0;

        bool falg=false;

        while(*str==' ')
        {
            str++;
        }

        if(*str=='-')//记录数字正负
        {
            falg=true;
            str++;
        }

        if(!(*str>='0'&&*str<='9'))//如果一开始非数字则退出，返回0.0
            return s;

        while(*str>='0'&&*str<='9'&&*str!='.')//计算小数点前整数部分
        {
            s=s*10.0+*str-'0';
            str++;
        }

        if(*str=='.')//以后为小树部分
            str++;

        while(*str>='0'&&*str<='9')//计算小数部分
        {
            s=s+(*str-'0')/d;
            d*=10.0;
            str++;
        }

        if(*str=='e'||*str=='E')//考虑科学计数法
        {
            str++;
            if(*str=='+')
            {
                str++;
                while(*str>='0'&&*str<='9')
                {
                    jishu=jishu*10+*str-'0';
                    str++;
                }
                while(jishu>0)
                {
                    s*=10;
                    jishu--;
                }
            }
            if(*str=='-')
            {
                str++;
                while(*str>='0'&&*str<='9')
                {
                    jishu=jishu*10+*str-'0';
                    str++;
                }
                while(jishu>0)
                {
                    s/=10;
                    jishu--;
                }
            }
        }

        return s*(falg?-1.0:1.0);
    }

    enum class FilterNodeType {
        LEAF = 0,
        BRANCH = 1,
        BOOSTING = 2,
    };

    class FilterNode {
    public:
        //得到名字
        const char *getName() { return name_; }

        //得到系数
        float getWeight() {
            return coff_;
        }

        int getWeightStr(char *weightStr) {
            sprintf(weightStr, "%.8f", coff_);
            int curIndex = strlen(weightStr);
            while (weightStr[curIndex - 1] == '0' || weightStr[curIndex - 1] == '.') {
                --curIndex;
                if (weightStr[curIndex] == '.') {
                    weightStr[curIndex] = 0;
                    break;
                }
            }
            weightStr[curIndex] = 0;
            return curIndex;
        }

        FilterNodeType getNodeType() {
            if (name_[0] == '+')
                return FilterNodeType::BOOSTING;
            if (leftId != -1)
                return FilterNodeType::BRANCH;
            return FilterNodeType::LEAF;
        }

        void setup(float weight = 0, const char *name = nullptr) {
            leftId = -1;
            rightId = -1;
            coff_ = weight;
            if (name != nullptr){
                int i = 0;
                while (name[i] != 0 && name[i] != ' ' && name[i] != '(' && name[i] != ')' && name[i] != ','){
                    name_[i] = name[i];
                    ++i;
                }
                name_[i] = 0;
            }
                //strcpy(name_, name);
            else
                name_[0] = 0;
        }

        int preId = {-1};
        int leftId = {-1};
        int rightId = {-1};
    protected:
        //过滤的系数
        float coff_ = {0};
        char name_[MAX_FILTER_NODE_NAME_LEN] = {0};
    };

    class BaseFilterTree {
    public:
        virtual ~BaseFilterTree() {
            nodeList_.clear();
        }

        virtual void clean() {
            nodeList_.resize(0);
        }

        int decode(const char *line, int l, int r) {
            int curIndex = l;
            if ((line[curIndex] >= '0' && line[curIndex] <= '9') || line[curIndex] == '-') {
                //解码叶节点
                int nodeId = createNode(atof(line));
                return nodeId;
            }
            if (line[curIndex] == '(' && line[r] == ')') {
                //int depth = 0;
                ++l;
                --r;
                curIndex = l;

                //找到?所在位置,并标记是否是boost节点
                int index_if = -1;
                int index_else = -1;
                int nodeId = -1;
                if (getIfElseIndex(line, l, r, index_if, index_else)) {
                    int leftId = decode(line, index_if + 2, index_else - 2);
                    int rightId = decode(line, index_else + 2, r);
                    CHECK(line[l] == '(' && line[index_if - 2] == ')', "format error!");
                    curIndex = l + 1;
                    while (curIndex < index_if && line[curIndex] != '<')
                        ++curIndex;
                    CHECK(line[curIndex] == '<', "format error!");

                    //读出系数
                    float coff = atof(line + (curIndex + 2));

                    //读出名字
                    nodeId = createNode(coff, line + (l + 1));
                    nodeList_[nodeId].leftId = leftId;
                    nodeList_[nodeId].rightId = rightId;
                    nodeList_[leftId].preId = nodeId;
                    nodeList_[rightId].preId = nodeId;
                } else if (getAddIndex(line, l, r, curIndex)) {
                    int leftId = decode(line, l, curIndex - 2);
                    int rightId = decode(line, curIndex + 2, r);
                    nodeId = createNode(0, "+");
                    nodeList_[nodeId].leftId = leftId;
                    nodeList_[nodeId].rightId = rightId;
                    nodeList_[leftId].preId = nodeId;
                    nodeList_[rightId].preId = nodeId;
                }

                return nodeId;
            } else {
                CHECK(false, "format error!");
            }
        }

        int encode(char *pout, int curIndex) {
            encode(pout, nodeList_.getSize() - 1, curIndex);
            return curIndex;
        }

    protected:


        void encode(char *pout, int nodeId, int &curIndex) {
            if (nodeList_[nodeId].getNodeType() == FilterNodeType::LEAF) {
                curIndex = nodeList_[nodeId].getWeightStr(pout + curIndex);
            } else {
                pout[curIndex++] = '(';
                if (nodeList_[nodeId].getNodeType() == FilterNodeType::BRANCH) {
                    pout[curIndex++] = '(';
                    strcpy(pout + curIndex, nodeList_[nodeId].getName());
                    curIndex += strlen(nodeList_[nodeId].getName());
                    strcpy(pout + curIndex, " < ");
                    curIndex += 3;
                    curIndex = nodeList_[nodeId].getWeightStr(pout + curIndex);
                    pout[curIndex++] = ')';

                    strcpy(pout + curIndex, " ? ");
                    curIndex += 3;
                    encode(pout, nodeList_[nodeId].leftId, curIndex);
                    strcpy(pout + curIndex, " : ");
                    curIndex += 3;
                    encode(pout, nodeList_[nodeId].rightId, curIndex);
                } else {
                    pout[curIndex++] = '(';
                    encode(pout, nodeList_[nodeId].leftId, curIndex);
                    strcpy(pout + curIndex, " + ");
                    curIndex += 3;
                    encode(pout, nodeList_[nodeId].rightId, curIndex);
                    pout[curIndex++] = ')';
                }

                pout[curIndex++] = ')';
            }
        }

        int createNode(float coff = 0, const char *name = nullptr) {
            int index = nodeList_.getSize();
            nodeList_[index].setup(coff, name);
            return index;
        }

        DArray<FilterNode, MAX_FILTER_NODE_BLOCK> nodeList_;
    private:
        //得到if else 语句符号位置,返回这个if else 语句是否属于boosting
        bool getIfElseIndex(const char *line, int l, int r, int &index_if, int &index_else) {
            bool isBoostNode = false;
            int curIndex = l;
            int depth = 0;
            while (curIndex < r) {
                if (line[curIndex] == '(')
                    ++depth;
                else if (line[curIndex] == ')')
                    --depth;
                else {
                    if (line[curIndex] == '?') {
                        if (depth != 0)
                            isBoostNode = true;
                        else
                            index_if = curIndex;
                    } else if (line[curIndex] == ':' && depth == 0) {
                        index_else = curIndex;
                        break;
                    }
                }
                ++curIndex;
            }
            return isBoostNode;
        }

        bool getAddIndex(const char *line, int l, int r, int &curIndex) {
            curIndex = l;
            int depth = 0;
            while (curIndex < r) {
                if (line[curIndex] == '(')
                    ++depth;
                else if (line[curIndex] == ')')
                    --depth;
                else if (depth == 0 && line[curIndex] == '+')
                    return line[curIndex] == '+';
                ++curIndex;
            }
            throw "format err";
        }
    };


}


#endif //ALPHATREE_BASEFILTERTREE_H

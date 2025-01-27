// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "Trie.hpp"
#include "Monitor.hpp"

#define super OSObject

OSDefineMetaClassAndStructors(Trie, OSObject)

Trie* Trie::create(TrieKind kind)
{
    Trie *instance = new Trie;
    if (instance != nullptr)
    {
        if (!instance->init(kind))
        {
            OSSafeReleaseNULL(instance);
        }
    }

    return instance;
}

bool Trie::init(TrieKind kind)
{
    if (!super::init())
    {
        return false;
    }

    size_ = 0;
    kind_ = mergeKindAndImpl(kind, g_bxl_enable_light_trie ? kLightTrie : kFastTrie);
    onChangeData_ = nullptr;
    onChangeCallback_ = nullptr;

    lock_ = IORecursiveLockAlloc();
    if (!lock_)
    {
        return false;
    }

    root_ = createNode(0);
    if (root_ == nullptr)
    {
        return false;
    }

    return true;
}

void Trie::free()
{
    traverse(/*computeKey*/ false, /*callbackArgs*/ nullptr, [](void*, uint64_t, Node *n)
             {
                 OSSafeReleaseNULL(n);
             });

    IORecursiveLockFree(lock_);
    lock_ = nullptr;
    root_ = nullptr;
    size_ = 0;

    super::free();
}

Trie::TrieResult Trie::makeSentinel(Node *node, void *factoryArgs, factory_fn factory)
{
    // if this is a sentinel node --> nothing to do
    if (node->record_ != nullptr)
    {
        return kTrieResultAlreadyExists;
    }

    OSObject *newRecord = factory(factoryArgs);
    if (newRecord == nullptr)
    {
        return kTrieResultAlreadyExists;
    }

    if (OSCompareAndSwapPtr(nullptr, newRecord, &node->record_))
    {
        // we updated 'record_' --> retain (by not releasing created newRecord) and increase trie size
        int oldCount = OSIncrementAtomic(&size_);
        triggerOnChange(oldCount, oldCount + 1);
        return kTrieResultInserted;
    }
    else
    {
        // someone else came first --> release 'newRecord' that we created for nothing
        OSSafeReleaseNULL(newRecord);
        return kTrieResultAlreadyExists;
    }
}

OSObject* Trie::get(Node *node)
{
    return node != nullptr ? node->record_ : nullptr;
}

OSObject* Trie::getOrAdd(Node *node, void *factoryArgs, factory_fn factory, TrieResult *result)
{
    if (node == nullptr) return nullptr;
    auto sentinelResult = makeSentinel(node, factoryArgs, factory);
    if (result) *result = sentinelResult;
    return node->record_;
}

Trie::TrieResult Trie::replace(Node *node, const OSObject *value)
{
    if (node == nullptr || value == nullptr)
    {
        return kTrieResultFailure;
    }

    OSObject *previousValue = node->record_;
    if (OSCompareAndSwapPtr(previousValue, (void*)value, &node->record_))
    {
        // we updated 'record_' --> retain the new value
        value->retain();

        if (previousValue != nullptr)
        {
            // this node was not empty --> release the previous record
            OSSafeReleaseNULL(previousValue);
            return kTrieResultReplaced;
        }
        else
        {
            // this node was previously empty --> increment size
            int oldCount = OSIncrementAtomic(&size_);
            triggerOnChange(oldCount, oldCount + 1);
            return kTrieResultInserted;
        }
    }
    else
    {
        // someone else came first --> declare race and do nothing
        return kTrieResultRace;
    }
}

Trie::TrieResult Trie::insert(Node *node, const OSObject *value)
{
    if (node == nullptr || value == nullptr)
    {
        return kTrieResultFailure;
    }

    if (OSCompareAndSwapPtr(nullptr, (void*)value, &node->record_))
    {
        // previous value was NULL and we updated 'record_' --> retain the new value and increment size
        value->retain();
        int oldCount = OSIncrementAtomic(&size_);
        triggerOnChange(oldCount, oldCount + 1);
        return kTrieResultInserted;
    }
    else
    {
        // the node was not empty or someone else came first --> bail and return "already exists"
        return kTrieResultAlreadyExists;
    }
}

Trie::TrieResult Trie::remove(Node *node)
{
    if (node == nullptr || node->record_ == nullptr)
    {
        return kTrieResultAlreadyEmpty;
    }

    OSObject *previousValue = node->record_;

    if (OSCompareAndSwapPtr(previousValue, nullptr, &node->record_))
    {
        // we updated record_ --> release previous value and decrease size
        OSSafeReleaseNULL(previousValue);
        int oldCount = OSDecrementAtomic(&size_);
        triggerOnChange(oldCount, oldCount - 1);
        return kTrieResultRemoved;
    }
    else
    {
        // someone else came first --> declare race and do nothing
        return kTrieResultRace;
    }
}

/*
 * Code used to generate this array:

 printf("static int s_char2idx[] = \n");
 printf("{\n");
 for (int ch = 0; ch < 256; ch++)
 {
     int idx = toupper(ch) - 32;
     if (idx < 0 || idx >= 65) idx = -1;
     printf("    %2d, // '%c' (\\%2d)\n", idx, ch < 32 || ch > 126 ? 0 : ch, ch);
 }
 printf("};\n");
 */
static int s_char2idx[256] =
{
    -1, // '' (\0)
    -1, // '' (\1)
    -1, // '' (\2)
    -1, // '' (\3)
    -1, // '' (\4)
    -1, // '' (\5)
    -1, // '' (\6)
    -1, // '' (\7)
    -1, // '' (\8)
    -1, // '' (\9)
    -1, // '' (\10)
    -1, // '' (\11)
    -1, // '' (\12)
    -1, // '' (\13)
    -1, // '' (\14)
    -1, // '' (\15)
    -1, // '' (\16)
    -1, // '' (\17)
    -1, // '' (\18)
    -1, // '' (\19)
    -1, // '' (\20)
    -1, // '' (\21)
    -1, // '' (\22)
    -1, // '' (\23)
    -1, // '' (\24)
    -1, // '' (\25)
    -1, // '' (\26)
    -1, // '' (\27)
    -1, // '' (\28)
    -1, // '' (\29)
    -1, // '' (\30)
    -1, // '' (\31)
    0, // ' ' (\32)
    1, // '!' (\33)
    2, // '"' (\34)
    3, // '#' (\35)
    4, // '$' (\36)
    5, // '%' (\37)
    6, // '&' (\38)
    7, // ''' (\39)
    8, // '(' (\40)
    9, // ')' (\41)
    10, // '*' (\42)
    11, // '+' (\43)
    12, // ',' (\44)
    13, // '-' (\45)
    14, // '.' (\46)
    15, // '/' (\47)
    16, // '0' (\48)
    17, // '1' (\49)
    18, // '2' (\50)
    19, // '3' (\51)
    20, // '4' (\52)
    21, // '5' (\53)
    22, // '6' (\54)
    23, // '7' (\55)
    24, // '8' (\56)
    25, // '9' (\57)
    26, // ':' (\58)
    27, // ';' (\59)
    28, // '<' (\60)
    29, // '=' (\61)
    30, // '>' (\62)
    31, // '?' (\63)
    32, // '@' (\64)
    33, // 'A' (\65)
    34, // 'B' (\66)
    35, // 'C' (\67)
    36, // 'D' (\68)
    37, // 'E' (\69)
    38, // 'F' (\70)
    39, // 'G' (\71)
    40, // 'H' (\72)
    41, // 'I' (\73)
    42, // 'J' (\74)
    43, // 'K' (\75)
    44, // 'L' (\76)
    45, // 'M' (\77)
    46, // 'N' (\78)
    47, // 'O' (\79)
    48, // 'P' (\80)
    49, // 'Q' (\81)
    50, // 'R' (\82)
    51, // 'S' (\83)
    52, // 'T' (\84)
    53, // 'U' (\85)
    54, // 'V' (\86)
    55, // 'W' (\87)
    56, // 'X' (\88)
    57, // 'Y' (\89)
    58, // 'Z' (\90)
    59, // '[' (\91)
    60, // '\' (\92)
    61, // ']' (\93)
    62, // '^' (\94)
    63, // '_' (\95)
    64, // '`' (\96)
    33, // 'a' (\97)
    34, // 'b' (\98)
    35, // 'c' (\99)
    36, // 'd' (\100)
    37, // 'e' (\101)
    38, // 'f' (\102)
    39, // 'g' (\103)
    40, // 'h' (\104)
    41, // 'i' (\105)
    42, // 'j' (\106)
    43, // 'k' (\107)
    44, // 'l' (\108)
    45, // 'm' (\109)
    46, // 'n' (\110)
    47, // 'o' (\111)
    48, // 'p' (\112)
    49, // 'q' (\113)
    50, // 'r' (\114)
    51, // 's' (\115)
    52, // 't' (\116)
    53, // 'u' (\117)
    54, // 'v' (\118)
    55, // 'w' (\119)
    56, // 'x' (\120)
    57, // 'y' (\121)
    58, // 'z' (\122)
    -1, // '{' (\123)
    -1, // '|' (\124)
    -1, // '}' (\125)
    -1, // '~' (\126)
    -1, // '' (\127)
    -1, // '' (\128)
    -1, // '' (\129)
    -1, // '' (\130)
    -1, // '' (\131)
    -1, // '' (\132)
    -1, // '' (\133)
    -1, // '' (\134)
    -1, // '' (\135)
    -1, // '' (\136)
    -1, // '' (\137)
    -1, // '' (\138)
    -1, // '' (\139)
    -1, // '' (\140)
    -1, // '' (\141)
    -1, // '' (\142)
    -1, // '' (\143)
    -1, // '' (\144)
    -1, // '' (\145)
    -1, // '' (\146)
    -1, // '' (\147)
    -1, // '' (\148)
    -1, // '' (\149)
    -1, // '' (\150)
    -1, // '' (\151)
    -1, // '' (\152)
    -1, // '' (\153)
    -1, // '' (\154)
    -1, // '' (\155)
    -1, // '' (\156)
    -1, // '' (\157)
    -1, // '' (\158)
    -1, // '' (\159)
    -1, // '' (\160)
    -1, // '' (\161)
    -1, // '' (\162)
    -1, // '' (\163)
    -1, // '' (\164)
    -1, // '' (\165)
    -1, // '' (\166)
    -1, // '' (\167)
    -1, // '' (\168)
    -1, // '' (\169)
    -1, // '' (\170)
    -1, // '' (\171)
    -1, // '' (\172)
    -1, // '' (\173)
    -1, // '' (\174)
    -1, // '' (\175)
    -1, // '' (\176)
    -1, // '' (\177)
    -1, // '' (\178)
    -1, // '' (\179)
    -1, // '' (\180)
    -1, // '' (\181)
    -1, // '' (\182)
    -1, // '' (\183)
    -1, // '' (\184)
    -1, // '' (\185)
    -1, // '' (\186)
    -1, // '' (\187)
    -1, // '' (\188)
    -1, // '' (\189)
    -1, // '' (\190)
    -1, // '' (\191)
    -1, // '' (\192)
    -1, // '' (\193)
    -1, // '' (\194)
    -1, // '' (\195)
    -1, // '' (\196)
    -1, // '' (\197)
    -1, // '' (\198)
    -1, // '' (\199)
    -1, // '' (\200)
    -1, // '' (\201)
    -1, // '' (\202)
    -1, // '' (\203)
    -1, // '' (\204)
    -1, // '' (\205)
    -1, // '' (\206)
    -1, // '' (\207)
    -1, // '' (\208)
    -1, // '' (\209)
    -1, // '' (\210)
    -1, // '' (\211)
    -1, // '' (\212)
    -1, // '' (\213)
    -1, // '' (\214)
    -1, // '' (\215)
    -1, // '' (\216)
    -1, // '' (\217)
    -1, // '' (\218)
    -1, // '' (\219)
    -1, // '' (\220)
    -1, // '' (\221)
    -1, // '' (\222)
    -1, // '' (\223)
    -1, // '' (\224)
    -1, // '' (\225)
    -1, // '' (\226)
    -1, // '' (\227)
    -1, // '' (\228)
    -1, // '' (\229)
    -1, // '' (\230)
    -1, // '' (\231)
    -1, // '' (\232)
    -1, // '' (\233)
    -1, // '' (\234)
    -1, // '' (\235)
    -1, // '' (\236)
    -1, // '' (\237)
    -1, // '' (\238)
    -1, // '' (\239)
    -1, // '' (\240)
    -1, // '' (\241)
    -1, // '' (\242)
    -1, // '' (\243)
    -1, // '' (\244)
    -1, // '' (\245)
    -1, // '' (\246)
    -1, // '' (\247)
    -1, // '' (\248)
    -1, // '' (\249)
    -1, // '' (\250)
    -1, // '' (\251)
    -1, // '' (\252)
    -1, // '' (\253)
    -1, // '' (\254)
    -1, // '' (\255)
};

static_assert(CHAR_BIT == 8, "char is not 8 bits long");
static_assert(UCHAR_MAX == 255, "max unsigned char is not 255");

Node* Trie::findPathNode(const char *path, bool createIfMissing)
{
    if (!isPathTrie())
    {
        log_error("Requested to find a path node in a non-path trie; tree kind: %d, path: %s", kind_, path);
        return nullptr;
    }

    Node *currNode = root_;
    unsigned char ch;
    while ((ch = *path++) != '\0')
    {
        int idx = s_char2idx[ch];
        if (idx < 0 || idx >= Node::s_pathNodeMaxKey)
        {
            return nullptr;
        }
        currNode = currNode->findChild(idx, createIfMissing, lock_);
        if (currNode == nullptr)
        {
            return nullptr;
        }
    }

    return currNode;
}

Node* Trie::findUintNode(uint64_t key, bool createIfMissing)
{
    if (!isUintTrie())
    {
        log_error("Requested to find a uint node in a non-uint trie; trie kind: %d, key: %lld", kind_, key);
        return nullptr;
    }

    Node *currNode = root_;
    while (true)
    {
        int lsd = key % 10;
        currNode = currNode->findChild(lsd, createIfMissing, lock_);
        if (!currNode)
        {
            return nullptr;
        }

        if (key < 10)
        {
            break;
        }

        key = key / 10;
    }

    return currNode;
}

bool Trie::onChange(void *callbackArgs, on_change_fn callback)
{
    if (onChangeCallback_) return false;
    onChangeData_ = callbackArgs;
    onChangeCallback_ = callback;
    return true;
}

void Trie::triggerOnChange(int oldCount, int newCount) const
{
    if (onChangeCallback_ && oldCount != newCount)
    {
        onChangeCallback_(onChangeData_, oldCount, newCount);
    }
}

void Trie::forEach(void *callbackArgs, for_each_fn callback)
{
    typedef struct { for_each_fn callback; void *args; } State;
    State state = { .callback = callback, .args = callbackArgs };
    traverse(/*computeKey*/ isUintTrie(), /*callbackArgs*/ &state, [](void *s, uint64_t key, Node *node)
             {
                 State *state = (State*)s;
                 OSObject *record = node->record_;
                 if (record)
                 {
                     record->retain();
                     state->callback(state->args, key, record);
                     record->release();
                 }
             });
}

void Trie::removeMatching(void *filterArgs, filter_fn filter)
{
    typedef struct { filter_fn filter; void *args; Trie *me; } State;
    State state = { .filter = filter, .args = filterArgs, .me = this };
    traverse(/*computeKey*/ false, /*callbackArgs*/ &state, [](void *s, uint64_t, Node *node)
             {
                 State *state = (State*)s;
                 OSObject *record = node->record_;
                 if (record)
                 {
                     record->retain();
                     if (state->filter(state->args, record))
                     {
                         state->me->remove(node);
                     }
                     record->release();
                 }
             });
}

void Trie::traverse(bool computeKey, void *callbackArgs, traverse_fn callback)
{
    root_->traverse(computeKey, callbackArgs, callback);
}

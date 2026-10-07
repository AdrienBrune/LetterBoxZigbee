#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <stdint.h>
#include <map>
#include <mutex>

#define DATA_BATTERY "battery"

class Data
{
public:
    enum class eType
    {
        ebool,  // 1 byte
        euint,  // 4 bytes
        efloat  // 4 bytes
    };
public:
    Data(eType type):
        mType(type)
    {}
    ~Data() = default;

    inline size_t GetSize() const
    {
        switch (mType)
        {
        case eType::euint:
            return sizeof(uint32_t);
        case eType::efloat:
            return sizeof(float);
        case eType::ebool:
            return sizeof(bool);
        default:
            return sizeof(uint32_t);
        }
    }

    inline eType GetType() const { return mType; }

    template<typename T>
    inline void SetValue(T value)
    {
        if constexpr (std::is_same_v<T, float>)
        {
            assert(mType == eType::efloat);
            mValue.f = value;
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            assert(mType == eType::ebool);
            mValue.b = value;
        }
        else
        {
            assert(mType == eType::euint);
            mValue.u = static_cast<uint32_t>(value);
        }
    }

    template<typename T>
    inline T GetValue() const
    {
        if constexpr (std::is_same_v<T, float>)
        {
            assert(mType == eType::efloat);
            return mValue.f;
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            assert(mType == eType::ebool);
            return mValue.b;
        }
        else if constexpr (std::is_integral_v<T>)
        {
            assert(mType == eType::euint);
            return static_cast<T>(mValue.u);
        }

        assert(false);
        return T();
    }

private:
    eType mType;
    union {
        uint32_t u;
        float f;
        bool b;
    } mValue;
};


class Memory
{
private:
    Memory()
    {
        _CreateAttribute(DATA_BATTERY, Data::eType::euint, (uint32_t)100);
    }

    template<typename T>
    inline void _CreateAttribute(std::string name, Data::eType type, T value)
    {
        mDataMap.insert(std::make_pair(name, Data(type)));
        auto it = mDataMap.find(name);
        if (it != mDataMap.end())
        {
            it->second.SetValue<T>(value); // set default value, use when no data is in memory
        }
    }

public:
    inline static Memory& GetMemory()
    {
        static Memory instance;
        return instance;
    }

    template<typename T>
    inline T Get(const std::string& name) const
    {
        std::lock_guard<std::mutex> lock(mMutex);

        auto it = mDataMap.find(name);
        if (it != mDataMap.end())
        {
            return it->second.GetValue<T>();
        }
        return T();
    }

    template<typename T>
    inline void Set(const std::string& name, T value)
    {
        { // mutex context
            std::lock_guard<std::mutex> lock(mMutex);

            auto it = mDataMap.find(name);
            if (it != mDataMap.end())
            {
                it->second.SetValue<T>(value);
                
            }
        }
        _Save();
    }

    void Load();

private:
    void _Save();

private:
    std::map<std::string, Data> mDataMap;
    mutable std::mutex mMutex;
};
#pragma once
#include <string>
namespace IO
{
    enum DriveType
    {
        UNSPECIFICIED = 0,
        RESERVED1 = 1,
        Reserved2 = 2,
        HDD = 3,
        SSD = 4,
        SCM = 5,
        FAIL = 6,
        COUNT
    };
    // sets up COM stuff, which has to be done ASAP
    // TODO this would be safer to do in a hook
    void Init();
    // NOTE: First call of thse for a drive are expensive, takes maybe 250ms
    DriveType GetDriveStorageType(std::string filePath);
}
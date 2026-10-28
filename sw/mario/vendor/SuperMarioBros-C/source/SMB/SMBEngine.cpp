#include <cstring>

#ifndef SMB_NO_AUDIO
#include "../Configuration.hpp"
#include "../Emulation/APU.hpp"
#endif
#include "../Emulation/Controller.hpp"
#include "../Emulation/PPU.hpp"

#include "SMBEngine.hpp"

#define DATA_STORAGE_OFFSET 0x8000 // Starting address for storing constant data

//---------------------------------------------------------------------
// Public interface
//---------------------------------------------------------------------

SMBEngine::SMBEngine(uint8_t* romImage) :
    a(*this, &registerA),
    x(*this, &registerX),
    y(*this, &registerY),
    s(*this, &registerS)
{
    c = z = n = false;
    registerA = registerX = registerY = registerS = 0;
    memset(dataStorage, 0, sizeof(dataStorage));
    memset(ram, 0, sizeof(ram));
#ifndef SMB_NO_AUDIO
    apu = new APU();
#else
    apu = nullptr;
#endif
    ppu = new PPU(*this);
    controller1 = new Controller();
    controller2 = new Controller();

    // CHR Location in ROM: Header (16 bytes) + 2 PRG pages (16k each)
#ifdef SMB_RV32I
    chr = romImage; // bare-metal passes the CHR array directly
#else
    chr = (romImage + 16 + (16384 * 2));
#endif

    returnIndexStackTop = 0;
}

SMBEngine::~SMBEngine()
{
#ifndef SMB_NO_AUDIO
    delete apu;
#endif
    delete ppu;
    delete controller1;
    delete controller2;
}

void SMBEngine::audioCallback(uint8_t* stream, int length)
{
#ifndef SMB_NO_AUDIO
    apu->output(stream, length);
#else
    (void)stream; (void)length;
#endif
}

Controller& SMBEngine::getController1()
{
    return *controller1;
}

Controller& SMBEngine::getController2()
{
    return *controller2;
}

void SMBEngine::render(uint32_t* buffer)
{
    ppu->render(buffer);
}

void SMBEngine::copyVideoState(uint8_t* state) { ppu->copyVideoState(state); }

void SMBEngine::reset()
{
    // Run the decompiled code for initialization
    code(0);
}

void SMBEngine::update()
{
    // Run the decompiled code for the NMI handler
    code(1);

    // Update the APU
#ifndef SMB_NO_AUDIO
    if (Configuration::getAudioEnabled())
    {
        apu->stepFrame();
    }
#endif
}

//---------------------------------------------------------------------
// Private methods
//---------------------------------------------------------------------

void SMBEngine::compare(uint8_t value1, uint8_t value2)
{
    uint8_t result = value1 - value2;
    c = (value1 >= value2);
    setZN(result);
}

void SMBEngine::bit(uint8_t value)
{
    n = (value & (1 << 7)) != 0;
    z = (registerA & value) == 0;
}

uint8_t* SMBEngine::getCHR()
{
    return chr;
}

uint8_t* SMBEngine::getDataPointer(uint16_t address)
{
    // Constant data
    if( address >= DATA_STORAGE_OFFSET )
    {
        return dataStorage + (address - DATA_STORAGE_OFFSET);
    }
    // RAM and Mirrors
    else if( address < 0x2000 )
    {
        return ram + (address & 0x7ff);
    }

    return nullptr;
}

MemoryAccess SMBEngine::getMemory(uint16_t address)
{
    uint8_t* dataPointer = getDataPointer(address);
    if( dataPointer != nullptr )
    {
        return MemoryAccess(*this, dataPointer);
    }
    else
    {
        return MemoryAccess(*this, readData(address));
    }
}

uint16_t SMBEngine::getMemoryWord(uint8_t address)
{
    return (uint16_t)readData(address) + ((uint16_t)(readData(address + 1)) << 8);
}

void SMBEngine::pha()
{
    writeData(0x100 | (uint16_t)registerS, registerA);
    registerS--;
}

void SMBEngine::pla()
{
    registerS++;
    a = readData(0x100 | (uint16_t)registerS);
}

int SMBEngine::popReturnIndex()
{
    return returnIndexStack[returnIndexStackTop--];
}

void SMBEngine::pushReturnIndex(int index)
{
    returnIndexStack[++returnIndexStackTop] = index;
}

uint8_t SMBEngine::readData(uint16_t address)
{
    // Constant data
    if( address >= DATA_STORAGE_OFFSET )
    {
        return dataStorage[address - DATA_STORAGE_OFFSET];
    }
    // RAM and Mirrors
    else if( address < 0x2000 )
    {
        return ram[address & 0x7ff];
    }
    // PPU Registers and Mirrors
    else if( address < 0x4000 )
    {
        return ppu->readRegister(0x2000 + (address & 0x7));
    }
    // IO registers
    else if( address < 0x4020 )
    {
        switch (address)
        {
        case 0x4016:
            return controller1->readByte();
        case 0x4017:
            return controller2->readByte();
        }
    }

    return 0;
}

void SMBEngine::setZN(uint8_t value)
{
    z = (value == 0);
    n = (value & (1 << 7)) != 0;
}

void SMBEngine::writeData(uint16_t address, uint8_t value)
{
    // RAM and Mirrors
    if( address < 0x2000 )
    {
        ram[address & 0x7ff] = value;
    }
    // PPU Registers and Mirrors
    else if( address < 0x4000 )
    {
        ppu->writeRegister(0x2000 + (address & 0x7), value);
    }
    // IO registers
    else if( address < 0x4020 )
    {
        switch( address )
        {
        case 0x4014:
            ppu->writeDMA(value);
            break;
        case 0x4016:
            controller1->writeByte(value);
            controller2->writeByte(value);
            break;
        default:
#ifndef SMB_NO_AUDIO
            apu->writeRegister(address, value);
#endif
            break;
        }
    }
}

void SMBEngine::writeData(uint16_t address, const uint8_t* data, size_t length)
{
    address -= DATA_STORAGE_OFFSET;

    memcpy(dataStorage + (std::ptrdiff_t)address, data, length);
}

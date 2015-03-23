/*
 *	DynamixelQ.cpp
 *
 *	Author: Andrew D. Horchler, adh9 @ case.edu
 *	Created: 8-13-14, modified: 3-23-15
 *
 *	Based on: Dynamixel.cpp by in2storm, 11-8-13
 */

#include "usb_type.h"
#include "DynamixelQ.h"

DynamixelQ::DynamixelQ(const byte DXL_Type)
{
	this->mDxlDevice = DXL_DEV1;
	this->mDxlUsart = USART1;
	
	this->mTxPort = PORT_DXL_TXD;
	this->mRxPort = PORT_DXL_RXD;
	this->mTxPin = PIN_DXL_TXD;
	this->mRxPin = PIN_DXL_RXD;
	
	this->mDirPort = PORT_TXRX_DIRECTION;
	this->mDirPin = PIN_TXRX_DIRECTION;
	
	switch (DXL_Type) {
		case 1:
		case 2:
		case 3:
			DXL_ADDRESS_TYPE[DXL_MULTI_TURN_OFFSET] = DXL_UNDEFINED_ADDRESS_TYPE;
			DXL_ADDRESS_TYPE[21] = DXL_UNDEFINED_ADDRESS_TYPE;
			DXL_ADDRESS_TYPE[DXL_RESOLUTION_DIVIDER] = DXL_UNDEFINED_ADDRESS_TYPE;
			
			DXL_ADDRESS_TYPE[DXL_CCW_COMPLIANCE_SLOPE] = DXL_BYTE_ADDRESS_TYPE;
			
			DXL_ADDRESS_TYPE[DXL_CURRENT] = DXL_UNDEFINED_ADDRESS_TYPE;
			DXL_ADDRESS_TYPE[DXL_TORQUE_CONTROL_MODE_ENABLE] = DXL_UNDEFINED_ADDRESS_TYPE;
			DXL_ADDRESS_TYPE[70] = DXL_UNDEFINED_ADDRESS_TYPE;
			DXL_ADDRESS_TYPE[DXL_GOAL_TORQUE] = DXL_UNDEFINED_ADDRESS_TYPE;
			DXL_ADDRESS_TYPE[72] = DXL_UNDEFINED_ADDRESS_TYPE;
			DXL_ADDRESS_TYPE[DXL_GOAL_ACCELERATION] = DXL_UNDEFINED_ADDRESS_TYPE;
			
			#ifndef DXL_MAX_BAUD
				#define DXL_MAX_BAUD DXL_AX_MAX_BAUD
			#elif DXL_MAX_BAUD > DXL_AX_MAX_BAUD
				#error DXL_MAX_BAUD greater than DXL_AX_MAX_BAUD
			#endif
			break;
			
		case 11:
		case 12:
			DXL_ADDRESS_TYPE[DXL_CURRENT] = DXL_UNDEFINED_ADDRESS_TYPE;
			DXL_ADDRESS_TYPE[DXL_TORQUE_CONTROL_MODE_ENABLE] = DXL_UNDEFINED_ADDRESS_TYPE;
			DXL_ADDRESS_TYPE[70] = DXL_UNDEFINED_ADDRESS_TYPE;
			DXL_ADDRESS_TYPE[DXL_GOAL_TORQUE] = DXL_UNDEFINED_ADDRESS_TYPE;
			DXL_ADDRESS_TYPE[72] = DXL_UNDEFINED_ADDRESS_TYPE;
			// Fall through
		case 13:
		case 14:
		case 10:
			#ifndef DXL_MAX_BAUD
				#define DXL_MAX_BAUD DXL_MX_MAX_BAUD
			#elif DXL_MAX_BAUD > DXL_MX_MAX_BAUD
				#error DXL_MAX_BAUD greater than DXL_MX_MAX_BAUD
			#endif
			break;
			
		default:
			#ifndef DXL_MAX_BAUD
				#define DXL_MAX_BAUD DXL_AX_MAX_BAUD
			#elif DXL_MAX_BAUD > DXL_AX_MAX_BAUD
				#error DXL_MAX_BAUD greater than DXL_AX_MAX_BAUD
			#endif
			break;
	}
}
DynamixelQ::~DynamixelQ(void) {
}

void DynamixelQ::begin(const byte baud)
{
	uint32 baudRate;
	
	baudRate = this->convertBaudRate(baud);
	if (baudRate != 0) {
		afio_remap(AFIO_REMAP_USART1);
		gpio_set_mode(this->mDirPort, this->mDirPin, GPIO_OUTPUT_PP);
		gpio_write_bit(this->mDirPort, this->mDirPin, 0); // RX Enable
		
		// initialize GPIO D20(PB6), D21(PB7) as DXL TX, RX respectively
		gpio_set_mode(this->mTxPort, this->mTxPin, GPIO_AF_OUTPUT_PP);
		gpio_set_mode(this->mRxPort, this->mRxPin, GPIO_INPUT_FLOATING);
		
		//Initialize USART 1 device
		usart_init(this->mDxlUsart);
		usart_set_baud_rate(this->mDxlUsart, STM32_PCLK2, baudRate);
		
		nvic_irq_set_priority(this->mDxlUsart->irq_num, 0);
		usart_attach_interrupt(this->mDxlUsart, this->mDxlDevice->handlers);
		usart_enable(this->mDxlUsart);
		delay_us(1e5);
		this->mDXLtxrxStatus = 0;
		
		this->clearBuffer();
	}
}


/*
 *	For the single bID, the byte value parameter stored at bAddress is returned.
 */
byte DynamixelQ::readByte(const byte bID, const byte bAddress)
{
	this->mParamBuffer[0] = bAddress;
	this->mParamBuffer[1] = DXL_BYTE_LENGTH;
	return (this->txRxPacket(bID, INST_READ, 2)) ? this->mRxBuffer[5] : DXL_INVALID_BYTE;
}

/*
 *	For each of the bIDLength IDs in bID, the byte value parameter stored at bAddress is
 *	returned in bData.
 */
void DynamixelQ::readByte(const byte bID[], const byte bIDLength, const byte bAddress, byte bData[])
{
	byte i;
	
	for (i = 0; i < bIDLength; i++) {
		bData[i] = this->readByte(bID[i], bAddress);
	}
}

/*
 *	For each of the bIDLength IDs in bID, the byte value parameter stored in the 
 *	corresponding bIDLength elements of the bAddress array is returned in bData.
 */
void DynamixelQ::readByte(const byte bID[], const byte bIDLength, const byte bAddress[], byte bData[])
{
	byte i;
	
	for (i = 0; i < bIDLength; i++) {
		bData[i] = this->readByte(bID[i], bAddress[i]);
	}
}


/*
 *	Write the byte value parameter in bData to the address bAddress of all actuators.
 */
byte DynamixelQ::writeByte(const byte bAddress, const byte bData)
{
	return this->writeByte(BROADCAST_ID, bAddress, bData);
}

/*
 *	For the single ID, bID, write the byte value parameter in bData to the address
 *	bAddress. 
 */
byte DynamixelQ::writeByte(const byte bID, const byte bAddress, const byte bData)
{
	this->mParamBuffer[0] = bAddress;
	this->mParamBuffer[1] = bData;
	return this->txRxPacket(bID, INST_WRITE, 2);
}

/*
 *	For each of the bIDLength IDs in bID, the byte value parameter stored in bData is
 *	written at address bAddress. If bIDLength is greater than DXL_MAX_IDS_PER_BYTE, only
 *	data for the first DXL_MAX_IDS_PER_BYTE IDs are transmitted so as not to exceed the
 *	maximum packet length, DXL_MAX_PACKET_LENGTH bytes.
 *	http://support.robotis.com/en/product/dynamixel/communication/dxl_instruction.htm#SYNC_WRITE
 */
byte DynamixelQ::writeByte(const byte bID[], const byte bIDLength, const byte bAddress, const byte bData)
{
	byte i, buff_idx = 1, lobyte;
	
	this->mParamBuffer[0] = bAddress;
	this->mParamBuffer[1] = DXL_BYTE_LENGTH;
	lobyte = DXL_LOBYTEQ(bData);
	for (i = 0; i < DXL_MIN(bIDLength, DXL_MAX_IDS_PER_BYTE); i++) {
		this->mParamBuffer[++buff_idx] = bID[i];
		this->mParamBuffer[++buff_idx] = lobyte;
	}
	return this->txRxPacket(BROADCAST_ID, INST_SYNC_WRITE, buff_idx+1);
}

/*
 *	For each of the bIDLength IDs in bID, the byte value parameters stored in the
 *	bIDLength bData array are written at address bAddress. If bIDLength is greater than
 *	DXL_MAX_IDS_PER_BYTE, only data for the first DXL_MAX_IDS_PER_BYTE IDs are transmitted
 *	so as not to exceed the maximum packet length, DXL_MAX_PACKET_LENGTH bytes.
 *	http://support.robotis.com/en/product/dynamixel/communication/dxl_instruction.htm#SYNC_WRITE
 */
byte DynamixelQ::writeByte(const byte bID[], const byte bIDLength, const byte bAddress, const byte bData[])
{
	byte i, buff_idx = 1;
	
	this->mParamBuffer[0] = bAddress;
	this->mParamBuffer[1] = DXL_BYTE_LENGTH;
  	for (i = 0; i < DXL_MIN(bIDLength, DXL_MAX_IDS_PER_BYTE); i++) {
		this->mParamBuffer[++buff_idx] = bID[i];
		this->mParamBuffer[++buff_idx] = DXL_LOBYTEQ(bData[i]);
	}
	return this->txRxPacket(BROADCAST_ID, INST_SYNC_WRITE, buff_idx+1);
}


/*
 *	For the single bID, the word value parameter stored at bAddress is returned.
 */
word DynamixelQ::readWord(const byte bID, const byte bAddress)
{
	this->mParamBuffer[0] = bAddress;
	this->mParamBuffer[1] = DXL_WORD_LENGTH;
	return (this->txRxPacket(bID, INST_READ, 2)) ? DXL_MAKEWORDQ(this->mRxBuffer[5], this->mRxBuffer[6]) : DXL_INVALID_WORD;
}

/*
 *	For each of the bIDLength IDs in bID, the word value parameter stored at bAddress is
 *	returned in wData.
 */
void DynamixelQ::readWord(const byte bID[], const byte bIDLength, const byte bAddress, word wData[])
{
	byte i;
	
	for (i = 0; i < bIDLength; i++) {
		wData[i] = this->readWord(bID[i], bAddress);	
	}
}

/*
 *	For each of the bIDLength IDs in bID, the word value parameter stored in the 
 *	corresponding bIDLength elements of the bAddress array is returned in wData.
 */
void DynamixelQ::readWord(const byte bID[], const byte bIDLength, const byte bAddress[], word wData[])
{
	byte i;
	
	for (i = 0; i < bIDLength; i++) {
		wData[i] = this->readWord(bID[i], bAddress[i]);	
	}
}

/*
 *	Write the word value parameter in wData to the address bAddress of all actuators.
 */
byte DynamixelQ::writeWord(const byte bAddress, const word wData)
{
	return this->writeWord(BROADCAST_ID, bAddress, wData);
}

/*
 *	For the single ID, bID, write the word value parameter in wData to the address
 *	bAddress. 
 */
byte DynamixelQ::writeWord(const byte bID, const byte bAddress, const word wData)
{
	this->mParamBuffer[0] = bAddress;
	this->mParamBuffer[1] = DXL_LOBYTEQ(wData);
	this->mParamBuffer[2] = DXL_HIBYTEQ(wData);
	return this->txRxPacket(bID, INST_WRITE, 3);
}

/*
 *	For each of the bIDLength IDs in bID, the word value parameter stored in wData is
 *	written at address bAddress. If bIDLength is greater than DXL_MAX_IDS_PER_WORD, only
 *	data for the first DXL_MAX_IDS_PER_WORD IDs are transmitted so as not to exceed the
 *	maximum packet length, DXL_MAX_PACKET_LENGTH bytes.
 *	http://support.robotis.com/en/product/dynamixel/communication/dxl_instruction.htm#SYNC_WRITE
 */
byte DynamixelQ::writeWord(const byte bID[], const byte bIDLength, const byte bAddress, const word wData)
{
	byte i, buff_idx = 1, lobyte, hibyte;
	
	this->mParamBuffer[0] = bAddress;
	this->mParamBuffer[1] = DXL_WORD_LENGTH;
	lobyte = DXL_LOBYTEQ(wData);
	hibyte = DXL_HIBYTEQ(wData);
	for (i = 0; i < DXL_MIN(bIDLength, DXL_MAX_IDS_PER_WORD); i++) {
		this->mParamBuffer[++buff_idx] = bID[i];
		this->mParamBuffer[++buff_idx] = lobyte;
		this->mParamBuffer[++buff_idx] = hibyte;
	}
	return this->txRxPacket(BROADCAST_ID, INST_SYNC_WRITE, buff_idx+1);
}

/*
 *	For each of the bIDLength IDs in bID, the word value parameters stored in the
 *	bIDLength wData array are written at address bAddress. If bIDLength is greater than
 *	DXL_MAX_IDS_PER_WORD, only data for the first DXL_MAX_IDS_PER_WORD IDs are transmitted
 *	so as not to exceed the maximum packet length, DXL_MAX_PACKET_LENGTH bytes.
 *	http://support.robotis.com/en/product/dynamixel/communication/dxl_instruction.htm#SYNC_WRITE
 */
byte DynamixelQ::writeWord(const byte bID[], const byte bIDLength, const byte bAddress, const word wData[])
{
	byte i, buff_idx = 1;
	
	this->mParamBuffer[0] = bAddress;
	this->mParamBuffer[1] = DXL_WORD_LENGTH;
	for (i = 0; i < DXL_MIN(bIDLength, DXL_MAX_IDS_PER_WORD); i++) {
		this->mParamBuffer[++buff_idx] = bID[i];
		this->mParamBuffer[++buff_idx] = DXL_LOBYTEQ(wData[i]);
		this->mParamBuffer[++buff_idx] = DXL_HIBYTEQ(wData[i]);
	}
	return this->txRxPacket(BROADCAST_ID, INST_SYNC_WRITE, buff_idx+1);
}


/*
 *	For a single ID, bID, bNumAddress parameter values are read from the monotonically
 *	increasing addresses in bAddress and written to wData. Each address is checked to see
 *	if it contains a byte or a word and the values are transformed accordingly before
 *	being written to wData. In the case of a failed read, wData will contain
 *	DXL_INVALID_WORD and/or DXL_INVALID_BYTE. Note that if bAddress contains
 *	non-consecutive addresses, any skipped addresses are still read, but not written to
 *	wData. This function does not actually use the the SYNC_READ instruction from
 *	Dynamixel protocol 2.0, but rather emulates its capabilities.
 */
void DynamixelQ::syncRead(const byte bID, const byte bStartAddress, const byte bNumAddress, word wData[])
{
	byte i, addr_len = 0, buff_idx = 5, addr_sz[bNumAddress];
	
	for (i = 0; i < bNumAddress; i++) {
		addr_sz[i] = this->isAddressWord(bStartAddress+addr_len) ? DXL_WORD_LENGTH : DXL_BYTE_LENGTH;
		addr_len += addr_sz[i];
	}
	if (bID < BROADCAST_ID) {
		if (bNumAddress == 1) {
			wData[0] = (addr_sz[0] == DXL_WORD_LENGTH) ? this->readWord(bID, bStartAddress) : this->readByte(bID, bStartAddress);
			return;
		} else {
			this->mParamBuffer[0] = bStartAddress;
			this->mParamBuffer[1] = addr_len;
			if (this->txRxPacket(bID, INST_READ, 2)) {
				for (i = 0; i < bNumAddress; i++) {
					if (addr_sz[i] == DXL_WORD_LENGTH) {
						wData[i] = DXL_MAKEWORDQ(this->mRxBuffer[buff_idx], this->mRxBuffer[buff_idx+1]);
						buff_idx += DXL_WORD_LENGTH;
					} else {
						wData[i] = this->mRxBuffer[buff_idx++];
					}
				}
				return;
			}
		}
	}
	
	for (i = 0; i < bNumAddress; i++) {
		wData[i] = (addr_sz[i] == DXL_WORD_LENGTH) ? DXL_INVALID_WORD : DXL_INVALID_BYTE;
	}
}


/*
 *	For each of the bIDLength IDs in bID, bNumAddress parameter values are read from the
 *	monotonically increasing addresses in bAddress and written to wData. If bAddressLength
 *	is equal to bNumAddress, the same bNumAddress addresses will be read from each bID.
 *	Otherwise, bAddressLength is assumed to equal bIDLength*bNumAddress and the bAddress
 *	array will be stepped through in blocks of bNumAddress for each bID. Each address is
 *	checked to see if it contains a byte or a word and the values are transformed
 *	accordingly before being written to wData. In the case of any failed reads, wData will
 *	contain DXL_INVALID_WORD and/or DXL_INVALID_BYTE at the corresponding locations. Note
 *	that if bAddress contains non-consecutive addresses, any skipped addresses are still
 *	read, but not written to wData. This function does not actually use the the SYNC_READ
 *	instruction, but rather emulates its capabilities.
 */
void DynamixelQ::syncRead(const byte bID[], byte bIDLength, const byte bStartAddress, const byte bNumAddress, word wData[])
{
	byte i, j, k, buff_idx = 0;
	
	for (i = 0; i < bIDLength; i++) {
		for (j = 0; j < i; j++) {
			if (bID[i] == bID[j]) {
				for (k = 0; k < bNumAddress; k++) {
					wData[buff_idx+k] = wData[j*bNumAddress+k];
				}
				break;
			}
		}
		
		if (i == j) {
			this->syncRead(bID[i], bStartAddress, bNumAddress, &wData[buff_idx]);
		}
		buff_idx += bNumAddress;
	}
}

/*
 *	The bNumDataPerID parameters stored in wData are written beginning at address
 *	bStartAddress to all actuators. Each address is checked to see if it contains a byte
 *	or a word and the values in wData are transformed accordingly. In the case of bytes
 *	(and invalid addresses), data values that overflow are masked to 0xFF). Note that this
 *	function does not actually use the SYNC_WRITE instruction as it is unnecessary for
 *	writing to a single ID.
 */
byte DynamixelQ::syncWrite(const byte bStartAddress, const word wData[], const byte bNumDataPerID)
{
	return this->syncWrite(BROADCAST_ID, bStartAddress, wData, bNumDataPerID);
}

/*
 *	For the single ID, bID, bNumDataPerID parameters stored in wData are written beginning
 *	at address bStartAddress. Each address is checked to see if it contains a byte or a
 *	word and the values in wData are transformed accordingly. In the case of bytes (and
 *	invalid addresses), data values that overflow are masked to 0xFF). Note that this
 *	function does not actually use the SYNC_WRITE instruction as it is unnecessary for
 *	writing to a single ID.
 */
byte DynamixelQ::syncWrite(const byte bID, const byte bStartAddress, const word wData[], const byte bNumData)
{
	byte i, buff_idx = 0;
	
	if (bNumData == 1) {
		if (this->isAddressWord(bStartAddress)) {
			return this->writeWord(bID, bStartAddress, wData[0]);
		} else if (this->isAddressByte(bStartAddress)) {
			return this->writeByte(bID, bStartAddress, (byte)wData[0]);
		} else {
			return 0;
		}
	} else {
		this->mParamBuffer[0] = bStartAddress;
		for (i = 0; i < bNumData; i++) {
			if (this->isAddressWord(bStartAddress+buff_idx)) {
				this->mParamBuffer[++buff_idx] = DXL_LOBYTEQ(wData[i]);
				this->mParamBuffer[++buff_idx] = DXL_HIBYTEQ(wData[i]);
			} else if (this->isAddressByte(bStartAddress+buff_idx)) {
				this->mParamBuffer[++buff_idx] = DXL_LOBYTEQ(wData[i]);
			}
		}
		return (buff_idx > 0) ? this->txRxPacket(bID, INST_WRITE, buff_idx+1) : 0;
	}
}

/*
 *	For each of the bIDLength IDs in bID, bNumDataPerID parameters stored in wData are
 *	written beginning at address bStartAddress. Each address is checked to see if it
 *	contains a byte or a word and the values in wData are transformed accordingly (in the
 *	case of bytes, data values that overflow are masked to 0xFF). The length of the array
 *	wData, bDataLength, may either be bNumDataPerID or bIDLength*bNumDataPerID. In the
 *	first case, the same bNumDataPerID values are written to each bID. Note that this
 *	function does not check if the the maximum packet length, DXL_MAX_PACKET_LENGTH bytes,
 *	is exceeded.
 *	http://support.robotis.com/en/product/dynamixel/communication/dxl_instruction.htm#SYNC_WRITE
 */
byte DynamixelQ::syncWrite(const byte bID[], const byte bIDLength, const byte bStartAddress, const word wData[], const byte bNumDataPerID, const byte bDataLength)
{	
	byte i, j, buff_idx = 2, addr_idx = bStartAddress, valid_idx;
	
	this->mParamBuffer[0] = bStartAddress;
	if (bNumDataPerID == bDataLength) {
		for (i = 0; i < bNumDataPerID; i++) {
			if (this->isAddressWord(addr_idx)) {
				this->mParamBuffer[++buff_idx] = DXL_LOBYTEQ(wData[i]);
				this->mParamBuffer[++buff_idx] = DXL_HIBYTEQ(wData[i]);
				addr_idx += DXL_WORD_LENGTH;
			} else if (this->isAddressByte(addr_idx)) {
				this->mParamBuffer[++buff_idx] = DXL_LOBYTEQ(wData[i]);
				addr_idx++;
			} else {
				addr_idx++;
			}
		}
		valid_idx = buff_idx;
		
		if (valid_idx > 2) {
			this->mParamBuffer[1] = bID[0];
			for (i = 1; i < bIDLength; i++) {
				this->mParamBuffer[++buff_idx] = bID[i];
				for (j = 3; j <= valid_idx; j++) {
					this->mParamBuffer[++buff_idx] = this->mParamBuffer[j];
				}
			}
		}
	} else {
		for (i = 0; i < bIDLength; i++) {
			for (j = 0; j < bNumDataPerID; j++) {
				if (this->isAddressWord(addr_idx)) {
					this->mParamBuffer[++buff_idx] = DXL_LOBYTEQ(wData[j]);
					this->mParamBuffer[++buff_idx] = DXL_HIBYTEQ(wData[j]);
					addr_idx += DXL_WORD_LENGTH;
				} else if (this->isAddressByte(addr_idx)) {
					this->mParamBuffer[++buff_idx] = DXL_LOBYTEQ(wData[j]);
					addr_idx++;
				} else {
					addr_idx++;
				}
			}
		}
	}
	this->mParamBuffer[1] = addr_idx-bStartAddress;
	
	return this->txRxPacket(BROADCAST_ID, INST_SYNC_WRITE, buff_idx+1);
}


/*
 *	PRIVATE MEMBER FUNCTIONS
 */
void DynamixelQ::writeRaw(const uint8 value)
{
	this->nsDelay(800);		// Delay to avoid locking/crashing in high-speed communication
	this->dxlTxEnable();	// Call dxlTxEnable() before write operations
	this->mDxlUsart->regs->DR = (value & DXL_USART_DR_PARITY_MASK);
	while ((this->mDxlUsart->regs->SR & USART_SR_TC) == RESET);
	this->dxlTxDisable();	//Call dxlTxDisable() after writing finished
}

void DynamixelQ::writeRaw(const uint8 *value, byte len)
{
	this->nsDelay(800);		// Delay to avoid locking/crashing in high-speed communication
	this->dxlTxEnable();
	while (len > 1) {
		this->mDxlUsart->regs->DR = (*value & DXL_USART_DR_PARITY_MASK);
		value++;
		while (!(this->mDxlUsart->regs->SR & USART_SR_TXE));
		len--;
	}
	this->mDxlUsart->regs->DR = (*value & DXL_USART_DR_PARITY_MASK);
	while ((this->mDxlUsart->regs->SR & USART_SR_TC) == RESET);
	this->dxlTxDisable();
}

void DynamixelQ::txPacket(byte bID, byte bInstruction, byte bParameterLength)
{
	byte bCount, bCheckSum;
	
	this->clearBuffer();
	bCheckSum = bID + bParameterLength+2 + bInstruction;
	this->mDxlDevice->read_pointer = this->mDxlDevice->write_pointer;
	
	this->mTxBuffer[0] = DXL_PACKET_HEADER;
	this->mTxBuffer[1] = DXL_PACKET_HEADER;
	this->mTxBuffer[2] = bID;
	this->mTxBuffer[3] = bParameterLength+2;
	this->mTxBuffer[4] = bInstruction;
	for (bCount = 0; bCount < bParameterLength; bCount++) {
		bCheckSum += this->mParamBuffer[bCount];
		this->mTxBuffer[5+bCount] = this->mParamBuffer[bCount];
	}
	this->mTxBuffer[5+bParameterLength] = (byte)(~bCheckSum);
	
	writeRaw(this->mTxBuffer, 6+bParameterLength);
	
	this->mDXLtxrxStatus = (1<<COMM_TXSUCCESS);
}

byte DynamixelQ::rxPacket(byte bID, byte bRxLength)
{
	unsigned long ulCounter;
	byte bCount, bLength, bChecksum;
	
	for (bCount = 0; bCount < bRxLength; bCount++) {
		ulCounter = 0;
		while (!this->available()) {
			this->nsDelay(0);
			if (ulCounter++ > RX_TIMEOUT_COUNT2) {
				this->mDXLtxrxStatus |= (1<<COMM_RXTIMEOUT);
				return 0;
			}
		}
		this->mRxBuffer[bCount] = readRaw();
	}
	
	bLength = bCount;
	if (bLength > 3) {
		bChecksum = 0;
		for (bCount = 2; bCount < bLength; bCount++) {
			bChecksum += this->mRxBuffer[bCount];
		}
		if ((this->mRxBuffer[0] != DXL_PACKET_HEADER) ||
				(this->mRxBuffer[1] != DXL_PACKET_HEADER) ||
				(this->mRxBuffer[this->mPktIdIndex] != bID) ||
				(this->mRxBuffer[this->mPktLengthIndex] != bLength-this->mPktInstIndex) ||
				((bChecksum & DXL_CHECKSUM_MASK) != DXL_CHECKSUM_MASK)) {
			this->mDXLtxrxStatus |= (1<<COMM_RXCORRUPT);
			return 0;
		}
	}
	return bLength;
}

/*
 *	Gateway function for reading and writing packets. Return 1 if successful, 0 otherwise.
 */
byte DynamixelQ::txRxPacket(byte bID, byte bInst, byte bTxParaLen)
{
	byte bRxLenEx;
	
	this->mDXLtxrxStatus = 0;
	this->txPacket(bID, bInst, bTxParaLen);
	
	if (bInst == INST_PING){
		bRxLenEx = (bID == BROADCAST_ID) ? 0xFF : DXL_PACKET_HEADER_LENGTH;
	} else if (bInst == INST_READ) {
		bRxLenEx = DXL_PACKET_HEADER_LENGTH+this->mParamBuffer[1];
	} else {
		bRxLenEx = (bID == BROADCAST_ID) ? 0 : DXL_PACKET_HEADER_LENGTH;
	}
	
	if (this->rxPacket(bID, bRxLenEx) != bRxLenEx) {
		return 0;
	} else {
		this->mDXLtxrxStatus = (1<<COMM_RXSUCCESS);
		return 1;
	}
}

void DynamixelQ::nsDelay(uint32 nsTime)
{
	uint32 i;
	static uint32 cnt = 0;
	
	for (i = 0; i < nsTime; i++) {
		cnt += i;
	}
}
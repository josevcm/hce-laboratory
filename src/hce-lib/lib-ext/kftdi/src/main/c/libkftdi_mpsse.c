/*	Benjamin DELPY `gentilkiwi`
	https://blog.gentilkiwi.com
	benjamin@gentilkiwi.com
	Licence : https://creativecommons.org/licenses/by/4.0/
*/
#include "libkftdi_mpsse.h"

FT_STATUS KFTDI_MPSSE_DirectCommand(FT_HANDLE ftHandle, const BYTE Command, const WORD Data, const BOOL isData)
{
	FT_STATUS status;
	BYTE Buffer[3];
	DWORD dwBytesWritten;

	Buffer[0] = Command;
	if (isData)
	{
		Buffer[1] = LOBYTE(Data);
		Buffer[2] = HIBYTE(Data);
	}

	FT_FAST_FAIL(FT_Write(ftHandle, Buffer, isData ? sizeof(Buffer) : 1, &dwBytesWritten));

FT_FAIL:
	return status;
}

FT_STATUS KFTDI_MPSSE_SPI_GPIO_AD_SetPinDirValue(PKFTDI_MPSSE_SPI_HANDLE pKFTDI, BYTE Pin, BYTE Dir, BYTE Value)
{
	FT_STATUS status;

	if (Dir)
	{
		pKFTDI->PinDirAD |= 1 << Pin;
	}
	else
	{
		pKFTDI->PinDirAD &= ~(1 << Pin);
	}

	if (Value)
	{
		pKFTDI->PinValAD |= 1 << Pin;
	}
	else
	{
		pKFTDI->PinValAD &= ~(1 << Pin);
	}

	FT_FAST_FAIL(KFTDI_MPSSE_DirectCommand(pKFTDI->ftHandle, KFTDI_MPSSE_CMD_GPIO_WRITE_AD, MAKEWORD(pKFTDI->PinValAD, pKFTDI->PinDirAD), TRUE));

FT_FAIL:
	return status;
}

FT_STATUS KFTDI_MPSSE_SPI_GPIO_AC_SetPinDirValue(PKFTDI_MPSSE_SPI_HANDLE pKFTDI, BYTE Pin, BYTE Dir, BYTE Value)
{
	FT_STATUS status;

	if (Dir)
	{
		pKFTDI->PinDirAC |= 1 << Pin;
	}
	else
	{
		pKFTDI->PinDirAC &= ~(1 << Pin);
	}

	if (Value)
	{
		pKFTDI->PinValAC |= 1 << Pin;
	}
	else
	{
		pKFTDI->PinValAC &= ~(1 << Pin);
	}

	FT_FAST_FAIL(KFTDI_MPSSE_DirectCommand(pKFTDI->ftHandle, KFTDI_MPSSE_CMD_GPIO_WRITE_AC, MAKEWORD(pKFTDI->PinValAC, pKFTDI->PinDirAC), TRUE));

FT_FAIL:
	return status;
}

FT_STATUS KFTDI_MPSSE_SPI_GPIO_AD_GetPins(PKFTDI_MPSSE_SPI_HANDLE pKFTDI, BYTE* pPins)
{
	return FT_GetBitMode(pKFTDI->ftHandle, pPins);
}

FT_STATUS KFTDI_MPSSE_SPI_GPIO_AD_GetPin(PKFTDI_MPSSE_SPI_HANDLE pKFTDI, BYTE Pin, BYTE* pValue)
{
	FT_STATUS status;
	BYTE pins;

	FT_FAST_FAIL(KFTDI_MPSSE_SPI_GPIO_AD_GetPins(pKFTDI, &pins));
	*pValue = (pins >> Pin) & 1;

FT_FAIL:
	return status;
}

FT_STATUS KFTDI_MPSSE_SPI_GPIO_AC_GetPins(PKFTDI_MPSSE_SPI_HANDLE pKFTDI, BYTE* pPins)
{
	FT_STATUS status;
	const BYTE cmd[2] = { KFTDI_MPSSE_CMD_GPIO_READ_AC, KFTDI_MPSSE_CMD_SEND_IMMEDIATE };
	DWORD dwHandled;

	FT_FAST_FAIL(FT_Write(pKFTDI->ftHandle, (LPVOID) cmd, sizeof(cmd), &dwHandled));
	FT_FAST_FAIL(FT_Read(pKFTDI->ftHandle, pPins, sizeof(*pPins), &dwHandled));

	if (dwHandled != sizeof(*pPins))
	{
		status = FT_IO_ERROR;
		goto FT_FAIL;
	}

FT_FAIL:
	return status;
}

FT_STATUS KFTDI_MPSSE_SPI_GPIO_AC_GetPin(PKFTDI_MPSSE_SPI_HANDLE pKFTDI, BYTE Pin, BYTE* pValue)
{
	FT_STATUS status;
	BYTE pins;

	FT_FAST_FAIL(KFTDI_MPSSE_SPI_GPIO_AC_GetPins(pKFTDI, &pins));
	*pValue = (pins >> Pin) & 1;

FT_FAIL:
	return status;
}

FT_STATUS KFTDI_MPSSE_SPI_Open(int deviceNumber, BYTE SpiMode, DWORD Frequency, PKFTDI_MPSSE_SPI_HANDLE pKFTDI)
{
	FT_STATUS status;
	BOOL bWasOpened = FALSE;

	if (!Frequency)
	{
		status = FT_INVALID_PARAMETER;
		goto FT_FAIL;
	}

	FT_FAST_FAIL(FT_Open(deviceNumber, &pKFTDI->ftHandle));
	bWasOpened = TRUE;

	FT_FAST_FAIL(FT_ResetDevice(pKFTDI->ftHandle));
	FT_FAST_FAIL(FT_Purge(pKFTDI->ftHandle, FT_PURGE_RX | FT_PURGE_TX));
	FT_FAST_FAIL(FT_SetUSBParameters(pKFTDI->ftHandle, 0x10000, 0x10000));
	FT_FAST_FAIL(FT_SetChars(pKFTDI->ftHandle, 0x00, 0, 0x00, 0));
	FT_FAST_FAIL(FT_SetTimeouts(pKFTDI->ftHandle, 5000, 5000));
	FT_FAST_FAIL(FT_SetLatencyTimer(pKFTDI->ftHandle, 10));

	FT_FAST_FAIL(FT_SetBitMode(pKFTDI->ftHandle, 0x00, FT_BITMODE_MPSSE));
	FT_FAST_FAIL(KFTDI_MPSSE_SPI_SetFrequency(pKFTDI, Frequency));

	pKFTDI->PinDirAD = PIN_DIR(PIN_SPI_CLK, PIN_OUTPUT) | PIN_DIR(PIN_SPI_MOSI, PIN_OUTPUT) | PIN_DIR(PIN_SPI_MISO, PIN_INPUT) | PIN_DIR(PIN_SPI_CS, PIN_OUTPUT);
	pKFTDI->PinValAD = PIN_VALUE(PIN_SPI_CLK, PIN_LOW) | PIN_VALUE(PIN_SPI_MOSI, PIN_LOW) | PIN_VALUE(PIN_SPI_CS, PIN_HIGH);
	pKFTDI->PinDirAC = 0x00;
	pKFTDI->PinValAC = 0x00;
	FT_FAST_FAIL(KFTDI_MPSSE_DirectCommand(pKFTDI->ftHandle, KFTDI_MPSSE_CMD_GPIO_WRITE_AD, MAKEWORD(pKFTDI->PinValAD, pKFTDI->PinDirAD), TRUE));
	pKFTDI->Options = SpiMode;

FT_FAIL:
	if (!FT_SUCCESS(status) && bWasOpened)
	{
		KFTDI_MPSSE_SPI_Close(pKFTDI);
	}
	return status;
}

FT_STATUS KFTDI_MPSSE_SPI_SetFrequency(PKFTDI_MPSSE_SPI_HANDLE pKFTDI, DWORD Frequency)
{
	FT_STATUS status;
	DWORD dwMaxFrequency;
	BYTE bDivisorCmd;

	if (!Frequency)
	{
		status = FT_INVALID_PARAMETER;
		goto FT_FAIL;
	}

	if (Frequency < KFTDI_MPSSE_MAX_FREQUENCY_WITH_DIV)
	{
		bDivisorCmd = KFTDI_MPSSE_CMD_ENABLE_CLK_DIV5;
		dwMaxFrequency = KFTDI_MPSSE_MAX_FREQUENCY_WITH_DIV;
	}
	else
	{
		bDivisorCmd = KFTDI_MPSSE_CMD_DISABLE_CLK_DIV5;
		dwMaxFrequency = KFTDI_MPSSE_MAX_FREQUENCY_WITHOUT_DIV;
	}
	FT_FAST_FAIL(KFTDI_MPSSE_DirectCommand(pKFTDI->ftHandle, bDivisorCmd, 0, FALSE));
	FT_FAST_FAIL(KFTDI_MPSSE_DirectCommand(pKFTDI->ftHandle, KFTDI_MPSSE_CMD_SET_CLOCK_DIVISOR, (WORD)((dwMaxFrequency / Frequency) - 1), TRUE));

FT_FAIL:
	return status;
}

FT_STATUS KFTDI_MPSSE_SPI_DataShift(PKFTDI_MPSSE_SPI_HANDLE pKFTDI, const BYTE* pcbOutData, BYTE* pbInData, WORD szData)
{
	FT_STATUS status;
	BYTE Buffer[4], cbCmd = sizeof(Buffer) - 1;
	DWORD dwBytesHandled;
	WORD wAdjustedSize;

	if (szData)
	{
		wAdjustedSize = szData - 1;
	}
	else
	{
		status = FT_INVALID_PARAMETER;
		goto FT_FAIL;
	}

	Buffer[0] = pKFTDI->Options | (pcbOutData ? KFTDI_MPSSE_DATA_SHIFTING_DO_WRITE : KFTDI_MPSSE_DATA_SHIFTING_DO_NOWRITE) | (pbInData ? KFTDI_MPSSE_DATA_SHIFTING_DI_READ : KFTDI_MPSSE_DATA_SHIFTING_DI_NOREAD);
	Buffer[1] = LOBYTE(wAdjustedSize);
	Buffer[2] = HIBYTE(wAdjustedSize);

	if (pbInData && !pcbOutData)
	{
		Buffer[3] = KFTDI_MPSSE_CMD_SEND_IMMEDIATE;
		cbCmd++;
	}

	FT_FAST_FAIL(FT_Write(pKFTDI->ftHandle, Buffer, cbCmd, &dwBytesHandled));
	if (pcbOutData)
	{
		FT_FAST_FAIL(FT_Write(pKFTDI->ftHandle, (LPVOID)pcbOutData, szData, &dwBytesHandled));
	}
	if (pbInData)
	{
		FT_FAST_FAIL(FT_Read(pKFTDI->ftHandle, pbInData, szData, &dwBytesHandled));
		if (dwBytesHandled != szData)
		{
			status = FT_IO_ERROR;
			goto FT_FAIL;
		}
	}

FT_FAIL:
	return status;
}

FT_STATUS KFTDI_MPSSE_SPI_DataShiftEx(PKFTDI_MPSSE_SPI_HANDLE pKFTDI, const BYTE* pcbOutData, BYTE* pbInData, DWORD dwSize)
{
	FT_STATUS status = FT_INVALID_PARAMETER;
	DWORD dwOffset = 0;
	WORD wChunkSize;

	for (dwOffset = 0; dwSize > 0; dwOffset += wChunkSize, dwSize -= wChunkSize)
	{
		wChunkSize = (dwSize > MAXWORD) ? MAXWORD : (WORD)dwSize;
		FT_FAST_FAIL(KFTDI_MPSSE_SPI_DataShift(pKFTDI, pcbOutData ? (pcbOutData + dwOffset) : NULL, pbInData ? (pbInData + dwOffset) : NULL, wChunkSize));
	}

FT_FAIL:
	return status;
}
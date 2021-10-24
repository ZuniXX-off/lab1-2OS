#include "copyFuncs.h"
#include <iostream>
#include <string>
#include <windows.h>

int callback = 0;

VOID CALLBACK FileIOCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
	callback++;
}

void MyCopyFile(const HANDLE& sourceHandle, const HANDLE& newFileHandle,
	size_t sizeBlock, size_t countOfOperations, DWORD lowOrderOfSize, DWORD highOrderOfSize) {

	int completedOperations = 0;
	sizeBlock *= 4096;

	uint64_t fullFileSize = (static_cast<uint64_t>(highOrderOfSize) << 32) + static_cast<uint64_t>(lowOrderOfSize);
	uint64_t shiftALL = static_cast<uint64_t>(countOfOperations) * static_cast<uint64_t>(sizeBlock);

	uint8_t** bufferAtOperation = new uint8_t * [countOfOperations];

	OVERLAPPED* overRead = new OVERLAPPED[countOfOperations];
	OVERLAPPED* overWrite = new OVERLAPPED[countOfOperations];

	for (int i = 0; i < countOfOperations; i++) {
		uint64_t shiftAtOperation = static_cast<uint64_t>(i) * static_cast<uint64_t>(sizeBlock);
		overRead[i].Offset = overWrite[i].Offset = static_cast<DWORD>((shiftAtOperation << 32) >> 32);
		overRead[i].OffsetHigh = overWrite[i].OffsetHigh = static_cast<DWORD>(shiftAtOperation >> 32);
		bufferAtOperation[i] = new uint8_t[sizeBlock];
	}

	do {
		callback = 0;
		completedOperations = 0;

		for (int i = 0; i < countOfOperations; ++i) {
			if (fullFileSize > 0) {
				completedOperations++;
				ReadFileEx(sourceHandle, bufferAtOperation[i], static_cast<DWORD>(sizeBlock), &overRead[i], FileIOCompletionRoutine);
				if (fullFileSize < sizeBlock) fullFileSize = sizeBlock;
				fullFileSize -= sizeBlock;
			}
		}

		while (callback < completedOperations) SleepEx(-1, TRUE);

		callback = 0;

		for (int i = 0; i < completedOperations; ++i) {
			WriteFileEx(newFileHandle, bufferAtOperation[i], sizeBlock, &overWrite[i], FileIOCompletionRoutine);
		}

		while (callback < completedOperations) SleepEx(-1, TRUE);

		for (int i = 0; i < countOfOperations; ++i) {
			uint64_t offsetUpdated = static_cast<uint64_t>(overRead[i].Offset)
				+ (static_cast<uint64_t>(overRead[i].OffsetHigh) << 32) + shiftALL;
			overRead[i].Offset = overWrite[i].Offset = static_cast<DWORD>((offsetUpdated << 32) >> 32);
			overRead[i].OffsetHigh = overWrite[i].OffsetHigh = static_cast<DWORD>(offsetUpdated >> 32);
		}

	} while (fullFileSize > 0);

	for (int i = 0; i < countOfOperations; ++i)
		delete[] bufferAtOperation[i];
	delete[] bufferAtOperation;
	delete[] overRead;
	delete[] overWrite;
}

void DoCopyFile() {
	size_t sizeBlock;
	size_t countOfOperations;
	std::string sourcePath;
	std::string newFilePath;
	HANDLE sourceHandle;
	HANDLE newFileHanlde;
	DWORD lowOrderOfSize;
	DWORD highOrderOfSize;
	

	std::cout << "Введите путь к файлу-источнику: ";
	std::getline(std::cin, sourcePath);
	sourceHandle = CreateFileA(sourcePath.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 
		FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
	if (sourceHandle == INVALID_HANDLE_VALUE) {
		std::cout << "Ошибка при открытии файла с кодом: " << GetLastError();
		return;
	}

	std::cout << "Введите путь размещения копии: ";
	std::getline(std::cin, newFilePath);
	newFileHanlde = CreateFileA(newFilePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
	if (newFileHanlde == INVALID_HANDLE_VALUE && GetLastError() != 0) {
		std::cout << "Ошибка при создании нового файла с кодом: " << GetLastError();
		CloseHandle(sourceHandle);
		return;
	}

	std::cout << "Введите размер блока: ";
	std::cin >> sizeBlock;

	std::cout << "Введите количество операций: ";
	std::cin >> countOfOperations;

	lowOrderOfSize = GetFileSize(sourceHandle, &highOrderOfSize);

	DWORD startTime = GetTickCount();
	MyCopyFile(sourceHandle, newFileHanlde, sizeBlock, countOfOperations, lowOrderOfSize, highOrderOfSize);
	DWORD endTime = GetTickCount();

	std::cout << "Затраченное время: " << endTime - startTime << " мс" << std::endl;

	long temp = static_cast<long>(highOrderOfSize);
	SetFilePointer(newFileHanlde, lowOrderOfSize, &temp, FILE_BEGIN);
	SetEndOfFile(newFileHanlde);
	CloseHandle(sourceHandle);
	CloseHandle(newFileHanlde);
}
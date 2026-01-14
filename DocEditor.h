#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>
#include <conio.h>

using namespace std;

/**
 * Editor Layout Configuration
 * Defines the dimensions and positioning for the console UI.
 */
const int page_start_Y = 2;
const int page_height = 20;
const int col_width = 35;
const int col1_start_X = 3;
const int col2_start_X = col1_start_X + col_width + 3;
const int page_end_X = col2_start_X + col_width + 3;
const int STATUS_BAR_Y = page_start_Y + page_height + 2;

// Storage limit: Each page holds two columns (Total lines = height * 2)
const int MAX_LINES_PER_PAGE_STORAGE = page_height * 2;

/**
 * DocumentPage Structure
 * Linked list node representing a single page in the document.
 */
struct DocumentPage {
    // Array to store individual lines of text
    string content[MAX_LINES_PER_PAGE_STORAGE];

    // Linked list pointers for navigation
    DocumentPage* next;
    DocumentPage* prev;

    // Unique index for mapping Undo/Redo operations
    int pageIndex;

    DocumentPage(int index = 0) : next(nullptr), prev(nullptr), pageIndex(index) {
        // Initialize all lines as empty strings
        for (int i = 0; i < MAX_LINES_PER_PAGE_STORAGE; ++i) {
            content[i] = "";
        }
    }
};

// Global pointers and counters
DocumentPage* headPage = nullptr;
DocumentPage* currentPagePtr = nullptr;
int nextPageGlobalIndex = 0;

// History Management: Fixed arrays for Undo/Redo (supports up to 100 pages)
const int history_depth = 10;
string undoStack[100][history_depth];
string redoStack[100][history_depth];
int undoTop[100];
int redoTop[100];

// Formatting and Search Constants
const char DELIMITER = '\n';
const int search_history_size = 5;

// Search History Tracking
string recentSearches[search_history_size];
int recentCount[search_history_size];
int searchHistoryTop = 0;
int searchHistoryTotal = 0;

// Editor State Flags
int currentAlignment = 0; // 0: Left, 1: Right, 2: Center, 3: Justify
bool isEncrypted = false;
string encryptionKey = "";

const char PAGE_DELIMITER = '\r';
const unsigned char CHECKSUM_MAGIC = 0xA9;

/**
 * Helper to calculate the 1-based display number of a page
 */
int getPageDisplayNumber(DocumentPage* page) {
    if (page == nullptr) return 0;
    int count = 1;
    DocumentPage* current = headPage;
    while (current != nullptr && current != page) {
        current = current->next;
        count++;
    }
    return (current == page) ? count : -1;
}

/**
 * Creates and appends a new page to the linked list
 */
DocumentPage* addNewPage() {
    // Limit check for fixed Undo/Redo array size
    if (nextPageGlobalIndex >= 100) return nullptr;

    DocumentPage* newPage = new DocumentPage(nextPageGlobalIndex);
    nextPageGlobalIndex++;

    if (headPage == nullptr) {
        headPage = newPage;
    }
    else {
        DocumentPage* lastPage = headPage;
        while (lastPage->next != nullptr) {
            lastPage = lastPage->next;
        }
        lastPage->next = newPage;
        newPage->prev = lastPage;
    }
    return newPage;
}

/**
 * Windows Console Management Functions
 */
void gotoxy(int x, int y) {
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void hideCursor() {
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    info.dwSize = 100;
    info.bVisible = FALSE;
    SetConsoleCursorInfo(consoleHandle, &info);
}

void showCursor() {
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    info.dwSize = 10;
    info.bVisible = TRUE;
    SetConsoleCursorInfo(consoleHandle, &info);
}

/**
 * Draws the visual framework of the editor
 */
void drawEditorUI(int currentPage) {
    system("cls");
    gotoxy(col1_start_X, 0);
    cout << "OUR FAST - WORD Editor (Welcome) ";
    gotoxy(page_end_X - 12, 0);
    cout << "Page: " << currentPage;
    gotoxy(col1_start_X, page_start_Y - 1);
    cout << "--- Column 1 ---";
    gotoxy(col2_start_X, page_start_Y - 1);
    cout << "--- Column 2 ---";

    for (int y = page_start_Y; y < page_start_Y + page_height; ++y) {
        gotoxy(col1_start_X - 2, y);
        cout << "|";
        gotoxy(col2_start_X - 2, y);
        cout << "|";
        gotoxy(col2_start_X + col_width, y);
        cout << "|";
    }
}

void clearStatusBar() {
    gotoxy(col1_start_X, STATUS_BAR_Y);
    cout << string(page_end_X, ' ');
}

string getAlignmentName() {
    if (currentAlignment == 1) return "RIGHT";
    if (currentAlignment == 2) return "CENTER";
    if (currentAlignment == 3) return "JUSTIFIED";
    return "LEFT";
}

void updateMainStatus(string message) {
    clearStatusBar();
    gotoxy(col1_start_X, STATUS_BAR_Y);
    cout << message;

    string alignName = getAlignmentName();
    string encName = isEncrypted ? "ENCRYPTED" : "PLAIN";
    string status = "[" + alignName + "] [" + encName + "]";

    gotoxy(page_end_X - status.length(), STATUS_BAR_Y);
    cout << status;
}

void updateMainStatusTemp(string tempMessage) {
    clearStatusBar();
    gotoxy(col1_start_X, STATUS_BAR_Y);
    cout << tempMessage;
}

void clearLine(int y) {
    gotoxy(0, y);
    cout << string(page_end_X + 10, ' ');
}

/**
 * Bitwise operations for Encryption/Security
 */
unsigned char RotL(unsigned char b, int n) {
    n = n & 7; return (b << n) | (b >> (8 - n));
}

unsigned char RotR(unsigned char b, int n) {
    n = n & 7; return (b >> n) | (b << (8 - n));
}

unsigned char getDynamicKey(const string& baseKey, int docLength, int pos) {
    unsigned char key = 'k';
    if (baseKey.length() > 0) {
        int keyIndex = pos;
        while (keyIndex >= baseKey.length()) keyIndex -= baseKey.length();
        key = baseKey[keyIndex];
    }
    key ^= (pos & 0xFF);
    key = RotL(key, (docLength & 3));
    key ^= ((pos >> 8) & 0xFF);
    return key;
}

unsigned char shuffleBits(unsigned char b) {
    unsigned char b0_7 = (b & 0x81), b1_6 = (b & 0x42), b_mid = (b & 0x3C);
    unsigned char swapped_b0_7 = ((b0_7 & 0x80) >> 7) | ((b0_7 & 0x01) << 7);
    unsigned char swapped_b1_6 = ((b1_6 & 0x40) >> 5) | ((b1_6 & 0x02) << 5);
    unsigned char b2 = (b_mid >> 2) & 1, b5 = (b_mid >> 5) & 1, xor_res = b2 ^ b5;
    b_mid &= 0xFB; b_mid |= (xor_res << 2);
    return swapped_b0_7 | swapped_b1_6 | b_mid;
}

unsigned char unshuffleBits(unsigned char b) {
    unsigned char b0_7 = (b & 0x81), b1_6 = (b & 0x42), b_mid = (b & 0x3C);
    unsigned char swapped_b0_7 = ((b0_7 & 0x80) >> 7) | ((b0_7 & 0x01) << 7);
    unsigned char swapped_b1_6 = ((b1_6 & 0x40) >> 5) | ((b1_6 & 0x02) << 5);
    unsigned char b5 = (b_mid >> 5) & 1, new_b2 = (b_mid >> 2) & 1, old_b2 = new_b2 ^ b5;
    b_mid &= 0xFB; b_mid |= (old_b2 << 2);
    return swapped_b0_7 | swapped_b1_6 | b_mid;
}

unsigned char calculateChecksum(const string& data) {
    unsigned char sum = 0; int len = data.length();
    for (int i = 0; i < len; ++i) sum ^= data[i];
    sum = RotL(sum, (len & 7)); sum ^= CHECKSUM_MAGIC;
    return sum;
}

bool isLikelyEncrypted(const string& data) {
    if (data.length() < 32) return false;
    long long onesCount = 0;
    long long dataLen = data.length();

    for (int i = 0; i < dataLen; ++i) {
        if ((data[i] & 1) != 0) {
            onesCount++;
        }
    }
    double onesRatio = (double)onesCount / dataLen;
    return (onesRatio >= 0.45 && onesRatio <= 0.55);
}

/**
 * Encryption and Decryption Engines
 */
string encrypt(string data, const string& baseKey) {
    int len = data.length(); if (len == 0) return "";
    unsigned char prev_cipher = 0;
    for (int i = 0; i < len; ++i) {
        unsigned char b = data[i];
        b = shuffleBits(b); b ^= getDynamicKey(baseKey, len, i);
        if ((i & 7) == 0) prev_cipher = 0;
        b ^= RotL(prev_cipher, 3);
        prev_cipher = b; data[i] = b;
    }
    return data;
}

string decrypt(string data, const string& baseKey) {
    int len = data.length(); if (len == 0) return "";
    unsigned char prev_cipher = 0;
    for (int i = 0; i < len; ++i) {
        unsigned char current_cipher = data[i];
        if ((i & 7) == 0) prev_cipher = 0;
        unsigned char b = current_cipher ^ RotL(prev_cipher, 3);
        b ^= getDynamicKey(baseKey, len, i); b = unshuffleBits(b);
        data[i] = b; prev_cipher = current_cipher;
    }
    return data;
}

/**
 * General User Input Handlers
 */
string getSimpleTextInput(int promptOffset) {
    string input = ""; char c; int y = STATUS_BAR_Y;
    gotoxy(promptOffset, y);
    showCursor();
    while (true) {
        c = _getch();
        if (c == 13) break;
        if (c == 8) {
            if (!input.empty()) {
                input = input.substr(0, input.length() - 1);
                gotoxy(promptOffset + input.length(), y);
                cout << " "; gotoxy(promptOffset + input.length(), y);
            }
        }
        else { input += c; cout << c; }
    }
    hideCursor();
    return input;
}

/**
 * Serialization Functions
 * Converts between memory objects and string formats for persistence.
 */
string serializePage(DocumentPage* pagePtr) {
    if (pagePtr == nullptr) return "";
    string snapshot = "";
    for (int i = 0; i < MAX_LINES_PER_PAGE_STORAGE; ++i) {
        snapshot += pagePtr->content[i];
        if (i < (MAX_LINES_PER_PAGE_STORAGE)-1) snapshot += DELIMITER;
    }
    return snapshot;
}

void deserializePage(DocumentPage* pagePtr, string data) {
    if (pagePtr == nullptr) return;
    for (int i = 0; i < MAX_LINES_PER_PAGE_STORAGE; ++i) pagePtr->content[i] = "";
    int lineIndex = 0, startPos = 0;
    for (int i = 0; i < data.length(); ++i) {
        if (data[i] == DELIMITER) {
            if (lineIndex < MAX_LINES_PER_PAGE_STORAGE) pagePtr->content[lineIndex] = data.substr(startPos, i - startPos);
            lineIndex++; startPos = i + 1;
        }
    }
    if (startPos < data.length() && lineIndex < MAX_LINES_PER_PAGE_STORAGE) pagePtr->content[lineIndex] = data.substr(startPos);
}

string serializeDocument() {
    string fullDocument = "";
    DocumentPage* current = headPage;
    while (current != nullptr) {
        fullDocument += serializePage(current);
        if (current->next != nullptr) fullDocument += PAGE_DELIMITER;
        current = current->next;
    }
    return fullDocument;
}

void deserializeDocument(string data) {
    DocumentPage* current = headPage;
    while (current != nullptr) {
        DocumentPage* next = current->next;
        delete current;
        current = next;
    }
    headPage = nullptr;
    currentPagePtr = nullptr;
    nextPageGlobalIndex = 0;

    int startPos = 0;
    for (int i = 0; i < data.length(); ++i) {
        if (data[i] == PAGE_DELIMITER) {
            DocumentPage* newPage = addNewPage();
            deserializePage(newPage, data.substr(startPos, i - startPos));
            startPos = i + 1;
        }
    }
    DocumentPage* lastPage = addNewPage();
    deserializePage(lastPage, data.substr(startPos));
    currentPagePtr = headPage;
}

/**
 * Undo and Redo Logic
 */
void clearRedo(int pageIndex) { redoTop[pageIndex] = -1; }

void pushUndo(int pageIndex) {
    if (pageIndex < 0 || pageIndex >= 100) return;
    if (undoTop[pageIndex] < history_depth - 1) undoTop[pageIndex]++;
    else for (int i = 0; i < history_depth - 1; i++) undoStack[pageIndex][i] = undoStack[pageIndex][i + 1];

    undoStack[pageIndex][undoTop[pageIndex]] = serializePage(currentPagePtr);
    clearRedo(pageIndex);
}

void pushRedo(int pageIndex) {
    if (pageIndex < 0 || pageIndex >= 100) return;
    if (redoTop[pageIndex] < history_depth - 1) redoTop[pageIndex]++;
    else for (int i = 0; i < history_depth - 1; i++) redoStack[pageIndex][i] = redoStack[pageIndex][i + 1];

    redoStack[pageIndex][redoTop[pageIndex]] = serializePage(currentPagePtr);
}

void pushUndoForRedo(int pageIndex) {
    if (pageIndex < 0 || pageIndex >= 100) return;
    if (undoTop[pageIndex] < history_depth - 1) undoTop[pageIndex]++;
    else for (int i = 0; i < history_depth - 1; ++i) undoStack[pageIndex][i] = undoStack[pageIndex][i + 1];
    undoStack[pageIndex][undoTop[pageIndex]] = serializePage(currentPagePtr);
}

string popUndo(int pageIndex) {
    if (pageIndex < 0 || pageIndex >= 100) return "";
    if (undoTop[pageIndex] == -1) return "";
    string state = undoStack[pageIndex][undoTop[pageIndex]];
    undoTop[pageIndex]--; return state;
}

string popRedo(int pageIndex) {
    if (pageIndex < 0 || pageIndex >= 100) return "";
    if (redoTop[pageIndex] == -1) return "";
    string state = redoStack[pageIndex][redoTop[pageIndex]];
    redoTop[pageIndex]--; return state;
}

/**
 * Search and Search History Management
 */
string toUpper(string s) {
    string result = "";
    for (int i = 0; i < s.length(); ++i) {
        if (s[i] >= 'a' && s[i] <= 'z') result += s[i] - 32;
        else result += s[i];
    }
    return result;
}

string getSearchTerm() {
    updateMainStatusTemp("Search Mode: Type word and press [Enter]. [Backspace] works.");
    int inputY = STATUS_BAR_Y + 2; gotoxy(0, inputY); cout << "Search: ";
    string term = ""; char c; showCursor();
    while (true) {
        c = _getch();
        if (c == 13) break;
        else if (c == 8) {
            if (!term.empty()) {
                term = term.substr(0, term.length() - 1);
                gotoxy(8 + term.length(), inputY); cout << " "; gotoxy(8 + term.length(), inputY);
            }
        }
        else { term += c; cout << c; }
    }
    hideCursor(); clearLine(inputY); return term;
}

int searchAndHighlight(string term) {
    if (currentPagePtr == nullptr || term.empty()) return 0;
    int matchCount = 0;
    string upperTerm = toUpper(term);

    for (int i = 0; i < MAX_LINES_PER_PAGE_STORAGE; ++i) {
        string line = currentPagePtr->content[i];
        string upperLine = toUpper(line);
        size_t pos = upperLine.find(upperTerm, 0);
        while (pos != string::npos) {
            matchCount++;
            pos = upperLine.find(upperTerm, pos + upperTerm.length());
        }
    }
    return matchCount;
}

void addSearchToHistory(string term, int matches) {
    recentSearches[searchHistoryTop] = term; recentCount[searchHistoryTop] = matches;
    searchHistoryTop = (searchHistoryTop + 1) % search_history_size;
    if (searchHistoryTotal < search_history_size) searchHistoryTotal++;
}

void displaySearchHistory() {
    int historyLineY = STATUS_BAR_Y + 1; clearLine(historyLineY);
    gotoxy(col1_start_X, historyLineY); cout << "History: ";
    int startIndex = (searchHistoryTop - 1 + search_history_size) % search_history_size;
    for (int i = 0; i < searchHistoryTotal; ++i) {
        int index = (startIndex - i + search_history_size) % search_history_size;
        cout << recentSearches[index] << "(" << recentCount[index] << ") ";
    }
}

/**
 * Text Formatting Engine (Alignment & Paragraph Processing)
 */
string applyAlignment(string line, bool isLastLineOfParagraph) {
    int padding = col_width - line.length();
    if (padding <= 0) return line;
    switch (currentAlignment) {
    case 0:
        return line;
    case 1:
        return string(padding, ' ') + line;
    case 2: {
        int l = padding / 2;
        return string(l, ' ') + line + string(padding - l, ' ');
    }
    case 3:
        if (isLastLineOfParagraph || line.find(' ') == string::npos) return line;

        string w[col_width];
        int wc = 0;
        string cw = "";
        int twl = 0;

        for (int i = 0; i < line.length(); ++i) {
            if (line[i] == ' ') {
                if (!cw.empty()) {
                    w[wc] = cw;
                    twl += cw.length();
                    wc++; cw = "";
                }
            }
            else {
                cw += line[i];
            }
        }

        if (!cw.empty()) {
            w[wc] = cw;
            twl += cw.length();
            wc++;
        }
        if (wc <= 1) return line;

        int g = wc - 1;
        int s = col_width - twl;
        int bs = s / g;
        int es = s % g;

        string jl = w[0];
        for (int i = 1; i < wc; ++i) {
            jl += string(bs, ' ');
            if (es > 0) { jl += ' '; es--; }
            jl += w[i];
        }
        return jl;
    }
    return line;
}

void processParagraph(string paragraph) {
    if (currentPagePtr == nullptr) return;
    int currentLineIndex = 0;
    while (currentLineIndex < MAX_LINES_PER_PAGE_STORAGE && !currentPagePtr->content[currentLineIndex].empty()) {
        currentLineIndex++;
    }
    if (currentLineIndex >= MAX_LINES_PER_PAGE_STORAGE) {
        updateMainStatusTemp("Page full - move to next page. Press any key."); _getch(); return;
    }
    string lineBuffer = currentPagePtr->content[currentLineIndex];
    string currentWord = "";
    paragraph += " ";
    for (int i = 0; i < paragraph.length(); ++i) {
        char c = paragraph[i];
        if (c == ' ' || c == '\n') {
            if (currentWord.empty()) continue;
            int spaceNeeded = (lineBuffer.empty() ? 0 : 1);
            if (lineBuffer.length() + spaceNeeded + currentWord.length() <= col_width) {
                lineBuffer += (spaceNeeded ? " " : "") + currentWord;
            }
            else {
                currentPagePtr->content[currentLineIndex] = applyAlignment(lineBuffer, false);
                currentLineIndex++;
                if (currentLineIndex >= MAX_LINES_PER_PAGE_STORAGE) {
                    updateMainStatusTemp("Page full. Word truncated. Press any key."); _getch();
                    currentWord = ""; break;
                }
                if (currentWord.length() > col_width) lineBuffer = currentWord.substr(0, col_width);
                else lineBuffer = currentWord;
            }
            currentWord = "";
        }
        else { currentWord += c; }
    }
    if (currentLineIndex < MAX_LINES_PER_PAGE_STORAGE && !lineBuffer.empty()) {
        currentPagePtr->content[currentLineIndex] = applyAlignment(lineBuffer, true);
    }
}

void handleTextInput(int currentPage) {
    if (currentPagePtr == nullptr) return;
    updateMainStatusTemp("Text Input Mode: Type paragraph, press [Enter] when done.");
    int inputY = STATUS_BAR_Y + 2;
    gotoxy(0, inputY); cout << "> ";
    string paragraph = ""; char c; showCursor();
    while (true) {
        c = _getch();
        if (c == 13) break;
        else if (c == 8) {
            if (!paragraph.empty()) {
                paragraph = paragraph.substr(0, paragraph.length() - 1);
                gotoxy(2 + paragraph.length(), inputY); cout << " "; gotoxy(2 + paragraph.length(), inputY);
            }
        }
        else { paragraph += c; cout << c; }
    }
    hideCursor(); clearLine(inputY);
    processParagraph(paragraph);
}

/**
 * File I/O and Document Persistence
 */
void clearAllUndoRedoStacks() {
    for (int i = 0; i < 100; ++i) {
        undoTop[i] = -1; redoTop[i] = -1;
    }
}

string readFile(string filename) {
    ifstream file(filename.c_str(), ios::binary);
    if (!file.is_open()) return "";
    file.seekg(0, ios::end); int length = file.tellg();
    file.seekg(0, ios::beg);
    if (length == 0) { file.close(); return ""; }
    char* buffer = new char[length];
    file.read(buffer, length);
    string data(buffer, length);
    delete[] buffer; file.close();
    return data;
}

void writeFile(string filename, string data) {
    ofstream file(filename.c_str(), ios::binary);
    if (!file.is_open()) return;
    file.write(data.c_str(), data.length());
    file.close();
}

void saveDocumentToFile() {
    updateMainStatusTemp("Enter filename to save: ");
    string filename = getSimpleTextInput(26);
    if (filename.empty()) return;
    if (!isEncrypted) {
        updateMainStatusTemp("Encrypting before save...");
        if (encryptionKey == "") {
            updateMainStatusTemp("Enter Encryption Key (seed): ");
            encryptionKey = getSimpleTextInput(28);
            if (encryptionKey == "") return;
        }
        string data = serializeDocument();
        string encrypted = encrypt(data, encryptionKey);
        unsigned char sum = calculateChecksum(encrypted);
        encrypted += sum;
        writeFile(filename, encrypted);
    }
    else {
        string data = serializeDocument();
        writeFile(filename, data);
    }
    updateMainStatusTemp("File saved securely! Press any key.");
    _getch();
}

bool loadDocumentFromFile(string mainStatus) {
    updateMainStatusTemp("Enter filename to open: ");
    string filename = getSimpleTextInput(25);
    if (filename.empty()) { updateMainStatus(mainStatus); return false; }
    string fullDoc = readFile(filename);
    if (fullDoc.empty()) {
        updateMainStatusTemp("File not found or empty. Press any key.");
        _getch(); updateMainStatus(mainStatus);
        return false;
    }
    clearAllUndoRedoStacks();

    bool loadSuccessful = false;
    if (isLikelyEncrypted(fullDoc) && fullDoc.length() > 0) {
        unsigned char storedSum = (unsigned char)fullDoc[fullDoc.length() - 1];
        string dataToCheck = fullDoc.substr(0, fullDoc.length() - 1);
        unsigned char initialChecksum = calculateChecksum(dataToCheck);

        if (initialChecksum == storedSum) {
            string currentKey = encryptionKey;
            if (currentKey == "") {
                updateMainStatusTemp("Encrypted file detected by analysis. Enter Key: ");
                currentKey = getSimpleTextInput(28);
            }

            string decryptedData = decrypt(dataToCheck, currentKey);
            string reEncrypted = encrypt(decryptedData, currentKey);
            unsigned char checkSumOnDecrypted = calculateChecksum(reEncrypted);

            if (checkSumOnDecrypted == storedSum) {
                deserializeDocument(decryptedData);
                isEncrypted = false;
                encryptionKey = currentKey;
                updateMainStatusTemp("Decrypted file loaded successfully. Press any key.");
                loadSuccessful = true;
            }
            else {
                updateMainStatusTemp("Decryption FAILED: Key mismatch or tampering detected. Press key.");
                loadSuccessful = false;
            }
        }
    }

    if (!loadSuccessful) {
        deserializeDocument(fullDoc);
        isEncrypted = false;
        encryptionKey = "";
        updateMainStatusTemp("Plain text (or corrupted) file loaded. Press any key.");
    }

    _getch();
    return true;
}

/**
 * Display and UI Rendering Logic
 */
string currentSearchTerm = "";
bool isSearchMode = false;

void displayPageContent(int currentPage) {
    if (currentPagePtr == nullptr) return;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    for (int y = 0; y < page_height; ++y) {
        string blankLine(col_width, ' ');
        gotoxy(col1_start_X, page_start_Y + y); cout << blankLine;
        gotoxy(col2_start_X, page_start_Y + y); cout << blankLine;
    }

    string allLines[MAX_LINES_PER_PAGE_STORAGE];
    bool isParaStart[MAX_LINES_PER_PAGE_STORAGE];
    int totalLines = 0;

    for (int l = 0; l < MAX_LINES_PER_PAGE_STORAGE; ++l) {
        string line = currentPagePtr->content[l];
        if (!line.empty()) {
            allLines[totalLines] = line;
            isParaStart[totalLines] = (l == 0) || (l == page_height) ||
                (l > 0 && currentPagePtr->content[l - 1].empty());
            totalLines++;
        }
    }

    if (totalLines == 0) return;

    int targetCol1Lines = (totalLines / 2) + (totalLines % 2);
    int splitIndex = targetCol1Lines;

    if (splitIndex > 0 && splitIndex < totalLines && !isParaStart[splitIndex]) {
        int paraStart = splitIndex - 1;
        while (paraStart > 0 && !isParaStart[paraStart]) paraStart--;
        int paraEnd = splitIndex;
        while (paraEnd < totalLines - 1 && !isParaStart[paraEnd + 1]) paraEnd++;

        int diff_A = abs(paraStart - (totalLines - paraStart));
        int diff_B = abs((paraEnd + 1) - (totalLines - (paraEnd + 1)));

        if (diff_A < diff_B) splitIndex = paraStart;
        else splitIndex = paraEnd + 1;
    }

    for (int i = 0; i < totalLines; ++i) {
        int startX = (i < splitIndex) ? col1_start_X : col2_start_X;
        int lineY = (i < splitIndex) ? i : (i - splitIndex);
        if (lineY >= page_height) continue;

        gotoxy(startX, page_start_Y + lineY);
        string line = allLines[i];

        if (isSearchMode && !currentSearchTerm.empty()) {
            string upperLine = toUpper(line);
            string upperTerm = toUpper(currentSearchTerm);
            size_t lastPos = 0;
            size_t foundPos = 0;

            while ((foundPos = upperLine.find(upperTerm, lastPos)) != string::npos) {
                cout << line.substr(lastPos, foundPos - lastPos);
                SetConsoleTextAttribute(hConsole, BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_INTENSITY);
                cout << line.substr(foundPos, currentSearchTerm.length());
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                lastPos = foundPos + currentSearchTerm.length();
            }
            cout << line.substr(lastPos);
        }
        else {
            cout << line;
        }
    }
}

/**
 * Table of Contents (TOC) Generator
 */
void handleTOCView(int currentPage, string mainStatus) {
    system("cls");
    gotoxy(3, 1);
    cout << "--- TABLE OF CONTENTS ---";

    string titles[500]; int pages[500]; int cols[500];
    int tocCount = 0;

    DocumentPage* current = headPage;
    while (current != nullptr) {
        int p = getPageDisplayNumber(current);
        for (int l = 0; l < MAX_LINES_PER_PAGE_STORAGE; ++l) {
            string line = current->content[l];
            if (!line.empty() && line[0] == '#') {
                if (tocCount < 500) {
                    titles[tocCount] = line.substr(1);
                    pages[tocCount] = p;
                    cols[tocCount] = (l < page_height) ? 1 : 2;
                    tocCount++;
                }
            }
        }
        current = current->next;
    }

    int y = 3;
    for (int i = 0; i < tocCount; ++i) {
        gotoxy(3, y + i);
        string title = to_string(i + 1) + ". " + titles[i];
        if (title.length() > 40) title = title.substr(0, 37) + "...";
        string location = "Page " + to_string(pages[i]) + ", Col " + to_string(cols[i]);
        cout << title;
        int dots = (page_end_X - 5) - title.length() - location.length();
        if (dots > 0) cout << string(dots, '.');
        cout << location;
    }
    if (tocCount == 0) {
        gotoxy(3, y);
        cout << "No headings found. (Start a line with # to create one.)";
    }

    gotoxy(3, y + tocCount + 2);
    cout << "Press any key to return to editor";
    _getch();

    drawEditorUI(currentPage);
    displayPageContent(currentPage);
    updateMainStatus(mainStatus);
}
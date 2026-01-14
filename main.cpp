#include "pch.h"
#include "DocEditor.h"
#include <iostream>
#include <string>
#include <windows.h>
#include <conio.h>
#include <vector>

using namespace std;

// --- Main Interactive Controller ---
int main() {
    // Stage 1: System Initialization
    hideCursor();

    // Start with a new, empty document using the Linked List
    if (headPage == nullptr) {
        currentPagePtr = addNewPage();
    }
    int currentPage = getPageDisplayNumber(currentPagePtr);

    bool editorRunning = true;
    // Professional Status Bar String
    string mainStatus = "[A] Add | [S] Search | [E] Encrypt | [V] Save | [O] Open | [I] Index | [U/R] | [N/P] | [L/T/C/J] | [Esc]";

    // Initialize Undo/Redo and Search History
    clearAllUndoRedoStacks();
    for (int i = 0; i < search_history_size; ++i) {
        recentSearches[i] = "";
        recentCount[i] = 0;
    }

    // Initial Screen Draw
    drawEditorUI(currentPage);
    displayPageContent(currentPage); // Recalculates column balance automatically
    updateMainStatus(mainStatus);

    // Stage 2: The Main Event Loop
    while (editorRunning) {
        char input = _getch();

        bool needsRedraw = false;    // For full UI redraw (borders + text)
        bool needsRebalance = false; // For just updating the text area

        bool pageChanged = false;
        bool contentChanged = false;

        // Map current page to index for Undo/Redo fixed arrays
        int pageIndex = (currentPagePtr != nullptr) ? currentPagePtr->pageIndex : -1;

        // Security Guard: Prevent editing while document is scrambled
        if (isEncrypted && (string("asuri").find(tolower(input)) != string::npos)) {
            updateMainStatusTemp("ACCESS DENIED: Document Encrypted. Press 'E' to Decrypt.");
            _getch();
            updateMainStatus(mainStatus);
            continue;
        }

        switch (input) {
            // --- Navigation Logic (Linked List Pointer Jumping) ---
        case 'n': case 'N':
            if (currentPagePtr != nullptr) {
                if (currentPagePtr->next != nullptr) {
                    currentPagePtr = currentPagePtr->next;
                }
                else {
                    DocumentPage* newPage = addNewPage();
                    if (newPage != nullptr) currentPagePtr = newPage;
                    else {
                        updateMainStatusTemp("System Error: Page Limit Reached.");
                        _getch();
                        break;
                    }
                }
                currentPage = getPageDisplayNumber(currentPagePtr);
                pageChanged = true;
            }
            break;

        case 'p': case 'P':
            if (currentPagePtr != nullptr && currentPagePtr->prev != nullptr) {
                currentPagePtr = currentPagePtr->prev;
                currentPage = getPageDisplayNumber(currentPagePtr);
                pageChanged = true;
            }
            break;

        // --- Editing Logic (Word-Wrapped Paragraphs) ---
        case 'a': case 'A':
            if (pageIndex != -1) pushUndo(pageIndex);
            handleTextInput(currentPage);
            contentChanged = true; // Triggers Smart Balancing
            updateMainStatus(mainStatus);
            break;

        case 's': case 'S': {
            if (currentPagePtr == nullptr) break;

            string term = getSearchTerm();
            if (!term.empty()) {
                // Activate Highlighting
                currentSearchTerm = term;
                isSearchMode = true;

                // Perform search to count matches for history
                int matches = searchAndHighlight(term); // This can still be used for counting
                addSearchToHistory(term, matches);

                // Update display with Yellow Highlights
                displayPageContent(currentPage);

                updateMainStatusTemp("Found " + to_string(matches) + " matches. Press any key to clear highlights.");
                displaySearchHistory();

                _getch(); // Wait for user to see highlights

                // Deactivate Highlighting
                isSearchMode = false;
                currentSearchTerm = "";

                // Final Redraw to clear the colors
                needsRebalance = true;
                clearLine(STATUS_BAR_Y + 1);
            }
            updateMainStatus(mainStatus);
            break;
        }

        // --- History Management (fixed-array stack access) ---
        case 'u': case 'U': {
            string state = popUndo(pageIndex);
            if (!state.empty()) {
                pushRedo(pageIndex);
                deserializePage(currentPagePtr, state);
                contentChanged = true;
            }
            else {
                updateMainStatusTemp("Undo Stack Empty.");
                _getch();
            }
            updateMainStatus(mainStatus);
            break;
        }

        case 'r': case 'R': {
            string state = popRedo(pageIndex);
            if (!state.empty()) {
                pushUndoForRedo(pageIndex);
                deserializePage(currentPagePtr, state);
                contentChanged = true;
            }
            else {
                updateMainStatusTemp("Redo Stack Empty.");
                _getch();
            }
            updateMainStatus(mainStatus);
            break;
        }

        // --- Security (Bitwise Shuffle & Block Cipher) ---
        case 'e': case 'E': {
            if (!isEncrypted) {
                updateMainStatusTemp("Enter Encryption Key to Scramble: ");
                encryptionKey = getSimpleTextInput(32);
                if (encryptionKey.empty()) {
                    updateMainStatus(mainStatus);
                    break;
                }

                // Scramble the entire linked list
                string docData = serializeDocument();
                string encryptedData = encrypt(docData, encryptionKey);
                deserializeDocument(encryptedData); // Rebuilds list with scrambled text

                isEncrypted = true;
                updateMainStatusTemp("Document Scrambled! Press any key.");
            }
            else {
                updateMainStatusTemp("Enter Key to Decrypt: ");
                string keyAttempt = getSimpleTextInput(22);

                // Despise the noise back into readable text
                string docData = serializeDocument();
                string decryptedData = decrypt(docData, keyAttempt);
                deserializeDocument(decryptedData); // Rebuilds list with clean text

                isEncrypted = false;
                updateMainStatusTemp("Document Restored! Press any key.");
            }
            _getch();
            pageChanged = true; // Force UI redraw
            break;
        }

        // --- Alignment & Layout ---
        case 'l': case 'L': currentAlignment = 0; contentChanged = true; break;
        case 't': case 'T': currentAlignment = 1; contentChanged = true; break;
        case 'c': case 'C': currentAlignment = 2; contentChanged = true; break;
        case 'j': case 'J': currentAlignment = 3; contentChanged = true; break;

        case 'i': case 'I': handleTOCView(currentPage, mainStatus); break;
        case 'v': case 'V': saveDocumentToFile(); updateMainStatus(mainStatus); break;
        case 'o': case 'O': if (loadDocumentFromFile(mainStatus)) pageChanged = true; break;

        case 27: // ESC key
            editorRunning = false;
            break;
        }

        // --- Optimized Rendering Engine ---
        if (pageChanged) {
            drawEditorUI(currentPage);
            displayPageContent(currentPage);
            updateMainStatus(mainStatus);
        }
        else if (contentChanged) {
            // Re-executes Smart Column Balancing for the current content
            displayPageContent(currentPage);
        }
    }

    // Stage 3: Graceful Shutdown (Memory Management)
    DocumentPage* current = headPage;
    while (current != nullptr) {
        DocumentPage* next = current->next;
        delete current;
        current = next;
    }

    return 0;
}
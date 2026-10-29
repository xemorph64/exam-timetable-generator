// =============================================================================
// Console.h - robust line-based input and optional ANSI colour output.
//
// Every read uses std::getline and parses the whole line, so non-numeric input
// can never leave std::cin in a failed state (the classic "cin >> int" bug):
// the bad line is discarded and the user is asked again. End of input
// (Ctrl+D / closed pipe) throws InputClosed so the program exits cleanly.
// =============================================================================
#pragma once

#include <stdexcept>
#include <string>
#include <vector>

class InputClosed : public std::runtime_error {
public:
    InputClosed() : std::runtime_error("input stream closed") {}
};

class Console {
public:
    std::string readLine(const std::string& prompt);  // may be empty
    // Non-empty text without the file separator '|'.
    std::string readText(const std::string& prompt, const std::string& what);
    int readInt(const std::string& prompt, int min, int max);
    long long readLong(const std::string& prompt, long long min, long long max);
    bool readYesNo(const std::string& prompt);
    void pause();

    void toggleColors() { colors_ = !colors_; }
    bool colorsEnabled() const { return colors_; }

    // Colour helpers - return plain text when colours are off.
    std::string slotColor(const std::string& text, int slotIndex) const;
    std::string good(const std::string& text) const;
    std::string bad(const std::string& text) const;
    std::string warn(const std::string& text) const;
    std::string bold(const std::string& text) const;

    // Layout helpers.
    void header(const std::string& title) const;
    void separator(char ch = '-', int width = 70) const;

private:
    std::string paint(const std::string& text, const char* code) const;
    bool colors_ = false;
};

// =============================================================================
// Console.cpp - input parsing and ANSI colouring.
// =============================================================================
#include "Console.h"

#include <iostream>
#include <string>

namespace {

const char* const kSlotPalette[] = {"\033[36m", "\033[33m", "\033[35m",
                                    "\033[32m", "\033[34m", "\033[91m"};
constexpr int kPaletteSize = static_cast<int>(sizeof(kSlotPalette) / sizeof(kSlotPalette[0]));

std::string trimmed(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool parseWholeNumber(const std::string& text, long long& value) {
    const std::string t = trimmed(text);
    if (t.empty()) return false;
    std::size_t used = 0;
    try {
        value = std::stoll(t, &used);
    } catch (const std::exception&) {
        return false;  // not a number, or out of range
    }
    return used == t.size();
}

}  // namespace

std::string Console::readLine(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) throw InputClosed();
    return trimmed(line);
}

std::string Console::readText(const std::string& prompt, const std::string& what) {
    while (true) {
        const std::string text = readLine(prompt);
        if (text.empty()) {
            std::cout << bad("  " + what + " cannot be empty.") << '\n';
        } else if (text.find('|') != std::string::npos) {
            std::cout << bad("  " + what + " cannot contain '|'.") << '\n';
        } else {
            return text;
        }
    }
}

long long Console::readLong(const std::string& prompt, long long min, long long max) {
    while (true) {
        long long value = 0;
        if (parseWholeNumber(readLine(prompt), value) && value >= min && value <= max) {
            return value;
        }
        std::cout << bad("  Please enter a whole number between " + std::to_string(min) +
                         " and " + std::to_string(max) + ".")
                  << '\n';
    }
}

int Console::readInt(const std::string& prompt, int min, int max) {
    return static_cast<int>(readLong(prompt, min, max));
}

bool Console::readYesNo(const std::string& prompt) {
    while (true) {
        const std::string answer = readLine(prompt + " (y/n): ");
        if (answer == "y" || answer == "Y" || answer == "yes" || answer == "YES") return true;
        if (answer == "n" || answer == "N" || answer == "no" || answer == "NO") return false;
        std::cout << bad("  Please answer y or n.") << '\n';
    }
}

void Console::pause() {
    readLine("\nPress Enter to continue...");
}

std::string Console::paint(const std::string& text, const char* code) const {
    return colors_ ? std::string(code) + text + "\033[0m" : text;
}

std::string Console::slotColor(const std::string& text, int slotIndex) const {
    if (slotIndex < 0) return text;
    return paint(text, kSlotPalette[slotIndex % kPaletteSize]);
}

std::string Console::good(const std::string& text) const { return paint(text, "\033[32m"); }
std::string Console::bad(const std::string& text) const { return paint(text, "\033[31m"); }
std::string Console::warn(const std::string& text) const { return paint(text, "\033[33m"); }
std::string Console::bold(const std::string& text) const { return paint(text, "\033[1m"); }

void Console::separator(char ch, int width) const {
    std::cout << std::string(static_cast<std::size_t>(width), ch) << '\n';
}

void Console::header(const std::string& title) const {
    std::cout << '\n';
    separator('=');
    std::cout << "  " << bold(title) << '\n';
    separator('=');
}

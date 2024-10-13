#include "json.h"
#include <iostream> // Для логирования

using namespace std;

namespace json {

namespace {

Node LoadNode(istream& input);

Node LoadNull(std::istream& input) {
    std::string text;
    char c;
    while (input.get(c) && isalpha(c)) {
        text += c;
    }
    input.putback(c);  // Возвращаем последний прочитанный символ (может быть разделителем)

    if (text == "null") {
        return Node(nullptr); 
    } else {
        throw ParsingError("Invalid null value");
    }
}

Node LoadBool(std::istream& input) {
    std::string text;
    char c;
    while (input.get(c) && isalpha(c)) {
        text += c;
    }
    input.putback(c);  // Возвращаем последний прочитанный символ (может быть разделителем)

    if (text == "true") {
        return Node{true};
    } else if (text == "false") {
        return Node{false};
    } else {
        throw ParsingError("Invalid boolean value");
    }
}

Node LoadArray(std::istream& input) {
    Array result;
    char c;
    
    while (input >> c) {
        if (c == ']') {  // Пустой массив
            return Node{result};
        } else {
            input.putback(c);
            result.push_back(LoadNode(input));  // Загрузка элемента массива
        }

        // Ожидаем запятую или закрывающую скобку
        input >> c;
        if (c == ']') {
            return Node{result};
        } else if (c != ',') {
            throw ParsingError("Expected ',' or ']' in array");
        }
    }
    
    throw ParsingError("Expected ']' at the end of array");
}

Node LoadNumber(std::istream& input) {
    using namespace std::literals;

    std::string parsed_num;

    // Считывает в parsed_num очередной символ из input
    auto read_char = [&parsed_num, &input] {
        parsed_num += static_cast<char>(input.get());
        if (!input) {
            throw ParsingError("Failed to read number from stream"s);
        }
    };

    // Считывает одну или более цифр в parsed_num из input
    auto read_digits = [&input, read_char] {
        if (!std::isdigit(input.peek())) {
            throw ParsingError("A digit is expected"s);
        }
        while (std::isdigit(input.peek())) {
            read_char();
        }
    };

    if (input.peek() == '-') {
        read_char();
    }
    // Парсим целую часть числа
    if (input.peek() == '0') {
        read_char();
    } else {
        read_digits();
    }

    bool is_int = true;
    // Парсим дробную часть числа
    if (input.peek() == '.') {
        read_char();
        read_digits();
        is_int = false;
    }

    // Парсим экспоненциальную часть числа
    if (int ch = input.peek(); ch == 'e' || ch == 'E') {
        read_char();
        if (ch = input.peek(); ch == '+' || ch == '-') {
            read_char();
        }
        read_digits();
        is_int = false;
    }

    try {
        if (is_int) {
            try {
                return Node(std::stoi(parsed_num));
            } catch (...) {
                // Если число слишком большое, пытаемся преобразовать в double
            }
        }
        return Node(std::stod(parsed_num));
    } catch (...) {
        throw ParsingError("Failed to convert "s + parsed_num + " to number"s);
    }
}

Node LoadString(std::istream& input) {
    using namespace std::literals;

    auto it = std::istreambuf_iterator<char>(input);
    auto end = std::istreambuf_iterator<char>();
    std::string s;
    while (true) {
        if (it == end) {
            throw ParsingError("String parsing error");
        }
        const char ch = *it;
        if (ch == '"') {
            ++it;
            break;
        } else if (ch == '\\') {
            ++it;
            if (it == end) {
                throw ParsingError("String parsing error");
            }
            const char escaped_char = *(it);
            switch (escaped_char) {
                case 'n': s.push_back('\n'); break;
                case 't': s.push_back('\t'); break;
                case 'r': s.push_back('\r'); break;
                case '"': s.push_back('"'); break;
                case '\\': s.push_back('\\'); break;
                default: throw ParsingError("Unrecognized escape sequence \\"s + escaped_char);
            }
        } else if (ch == '\n' || ch == '\r') {
            throw ParsingError("Unexpected end of line"s);
        } else {
            s.push_back(ch);
        }
        ++it;
    }

    return Node(s);
}

Node LoadDict(istream& input) {
    Dict result;
    char c;
    while (input >> c) {
        if (c == '}') {
            break;  // Пустой словарь
        }
        if (c == ',') {
            input >> c;
        }
        input.putback(c);
        
        string key = LoadNode(input).AsString();
        input >> c;  // Пропускаем ':'
        result.insert({move(key), LoadNode(input)});
        
        input >> c;  // Пропускаем ',' или '}'
        if (c == '}') {
            break;
        }
        if (c != ',') {
            throw ParsingError("Expected ',' or '}' in dict");
        }
    }

    return Node(move(result));
}

Node LoadNode(std::istream& input) {
    char c;
    input >> c;

    // Логирование символа
    //std::cout << "Parsing character: " << c << std::endl;

    if (c == '[') {
        return LoadArray(input);
    } else if (c == '{') {
        return LoadDict(input);
    } else if (c == '"') {
        return LoadString(input);
    } else if (c == 't' || c == 'f') {
        input.putback(c);
        return LoadBool(input);
    } else if (c == 'n') {
        input.putback(c);
        return LoadNull(input);
    } else if (c == '-' || isdigit(c)){
        input.putback(c);
        return LoadNumber(input);
    } else {
        throw ParsingError("Invalid data");
    }
}

}  // namespace

Node::Node() = default;
Node::Node(Array array) { *this = std::move(array); }
Node::Node(Dict map) { *this = std::move(map); }
Node::Node(int value) { *this = value; }
Node::Node(double value) { *this = value; }
Node::Node(std::string value) { *this = std::move(value); }
Node::Node(bool value) {*this = value;}
Node::Node(std::nullptr_t) { *this = nullptr; } 

bool Node::IsNull() const { return std::holds_alternative<std::nullptr_t>(*this); }
bool Node::IsArray() const { return std::holds_alternative<Array>(*this); }
bool Node::IsMap() const { return std::holds_alternative<Dict>(*this); }
bool Node::IsBool() const { return std::holds_alternative<bool>(*this); }
bool Node::IsInt() const { return std::holds_alternative<int>(*this); }
bool Node::IsDouble() const { return std::holds_alternative<double>(*this) || std::holds_alternative<int>(*this); }
bool Node::IsPureDouble() const { return std::holds_alternative<double>(*this); }
bool Node::IsString() const { return std::holds_alternative<std::string>(*this); }

const Array& Node::AsArray() const {
    if (std::holds_alternative<Array>(*this)) return std::get<Array>(*this);
    throw std::logic_error("Not an array");
}

const Dict& Node::AsMap() const {
    if (std::holds_alternative<Dict>(*this)) { return std::get<Dict>(*this); }
    throw std::logic_error("Not a map");
}

bool Node::AsBool() const { 
    if (std::holds_alternative<bool>(*this))  { return std::get<bool>(*this); }
    throw std::logic_error("Not a bool");
}

int Node::AsInt() const {
    if (std::holds_alternative<int>(*this))  { return std::get<int>(*this); }
    throw std::logic_error("Not an int");
}

double Node::AsDouble() const {
    if (std::holds_alternative<double>(*this)) { return std::get<double>(*this); }
    if (std::holds_alternative<int>(*this)) { return static_cast<double>(std::get<int>(*this)); }
    throw std::logic_error("Not a double");
}

const string& Node::AsString() const {
    if (std::holds_alternative<std::string>(*this)) { return std::get<string>(*this); }
    throw std::logic_error("Not a string");
}

const Node::Value& Node::GetValue() const {
    return *this;
}

Document::Document(Node root)
    : root_(move(root)) {
}

const Node& Document::GetRoot() const {
    return root_;
}

Document Load(istream& input) {
    return Document{LoadNode(input)};
}

void PrintString(const std::string& str, std::ostream& output) {
    output << '"';
    for (const char c : str) {
        switch (c) {
            case '\n':
                output << "\\n";
                break;
            case '\t':
                output << "\\t";
                break;
            case '\r':
                output << "\\r";
                break;
            case '"':
                output << "\\\"";
                break;
            case '\\':
                output << "\\\\";
                break;
            default:
                output << c;
                break;
        }
    }
    output << '"';
}

void PrintNode(const Node& node, std::ostream& output) {
    if (node.IsNull()) {
        output << "null";
    } else if (node.IsBool()) {
        output << (node.AsBool() ? "true" : "false");
    } else if (node.IsInt()) {
        output << node.AsInt();
    } else if (node.IsDouble()) {
        output << node.AsDouble();
    } else if (node.IsString()) {
        PrintString(node.AsString(), output);
    } else if (node.IsArray()) {
        const auto& array = node.AsArray();
        output << '[';
        for (size_t i = 0; i < array.size(); ++i) {
            PrintNode(array[i], output);
            if (i < array.size() - 1) {
                output << ',';
            }
        }
        output << ']';
    } else if (node.IsMap()) {
        const auto& dict = node.AsMap();
        output << '{';
        for (auto it = dict.begin(); it != dict.end(); ++it) {
            output << '"' << it->first << "\": ";
            PrintNode(it->second, output);
            if (std::next(it) != dict.end()) {
                output << ',';
            }
        }
        output << '}';
    }
}

void Print(const Document& doc, std::ostream& output) {
    PrintNode(doc.GetRoot(), output);
}

}  // namespace json

// Port of the provided C++ console calculator to Swift
// Features:
//  - Modes: inline single (-s), inline multiple (-m), argument mode (-a)
//  - Functions: sqrt, log (natural), ln (alias), sin, cos, tan
//  - Operators: + - * / ^ with precedence and associativity
//  - Parentheses and error handling
//  - Appends expressions to ~/.calchistory

import Foundation

enum Mode {
	case argument
	case inlineSingle
	case inlineMultiple
}

let precedence: [Character: Int] = [
	"+": 1, "-": 1,
	"*": 2, "/": 2,
	"^": 3
]

// Apply binary operators: +, -, *, /, ^
func applyOperation(_ a: Double, _ b: Double, _ op: Character) throws -> Double {
	switch op {
	case "+": return a + b
	case "-": return a - b
	case "*": return a * b
	case "/":
		if b == 0.0 { throw NSError(domain: "CalcError", code: 1, userInfo: [NSLocalizedDescriptionKey: "Error: Division by zero!"]) }
		return a / b
	case "^": return pow(a, b)
	default:
		throw NSError(domain: "CalcError", code: 2, userInfo: [NSLocalizedDescriptionKey: "Error: Unknown operator '\(op)'!"]) }
}

// Apply unary functions: sqrt, log, ln, sin, cos, tan
func applyFunction(_ fn: String, _ x: Double) throws -> Double {
	switch fn {
	case "sqrt": return sqrt(x)
	case "log", "ln":
		if x <= 0.0 { throw NSError(domain: "CalcError", code: 3, userInfo: [NSLocalizedDescriptionKey: "Error: log domain error!"]) }
		return log(x) // natural log
	case "sin": return sin(x)
	case "cos": return cos(x)
	case "tan": return tan(x)
	default:
		throw NSError(domain: "CalcError", code: 4, userInfo: [NSLocalizedDescriptionKey: "Error: Unknown function '\(fn)'!"]) }
}

func checkParentheses(_ s: String) -> Bool {
	var bal = 0
	for ch in s {
		if ch == "(" { bal += 1 }
		else if ch == ")" { bal -= 1; if bal < 0 { return false } }
	}
	return bal == 0
}

// Tokenize into numbers, identifiers, operators and parentheses
func tokenize(_ eq: String) -> [String] {
	var tokens: [String] = []
	var cur = ""
	let chars = Array(eq)
	var i = 0

	func flush() {
		if !cur.isEmpty { tokens.append(cur); cur = "" }
	}

	while i < chars.count {
		let ch = chars[i]
		if ch.isWhitespace { i += 1; continue }

		if ch.isLetter {
			flush()
			var name = String(ch); i += 1
			while i < chars.count && chars[i].isLetter { name.append(chars[i]); i += 1 }
			tokens.append(name)
			continue
		}

		// number or unary minus
		let prevIsOp: Bool = {
			if i == 0 { return true }
			let prev = chars[i-1]
			if prev == "(" { return true }
			if precedence[prev] != nil { return true }
			return false
		}()
		let isUnaryMinus = (ch == "-" && prevIsOp)
		if ch.isNumber || ch == "." || isUnaryMinus {
			cur.append(ch); i += 1
			while i < chars.count && (chars[i].isNumber || chars[i] == ".") { cur.append(chars[i]); i += 1 }
			flush()
			continue
		}

		// single-char tokens (operators, parens, commas)
		flush()
		tokens.append(String(ch))
		i += 1
	}
	flush()
	return tokens
}

// Evaluate expression with recursion for parentheses and functions
func evaluateExpression(_ it: inout IndexingIterator<[String]>) throws -> Double {
	var nums: [Double] = []
	var ops: [Character] = []

	func applyTopOp() throws {
		guard let top = ops.popLast() else { throw NSError(domain: "CalcError", code: 5, userInfo: [NSLocalizedDescriptionKey: "Error: Operator stack empty"]) }
		guard nums.count >= 2 else { throw NSError(domain: "CalcError", code: 6, userInfo: [NSLocalizedDescriptionKey: "Error: Not enough operands"]) }
		let b = nums.removeLast(); let a = nums.removeLast()
		let res = try applyOperation(a, b, top)
		nums.append(res)
	}

	while true {
		guard let tok = it.next() else { break }
		if tok == ")" { break }

		if let first = tok.first, first.isLetter {
			// function call: fn ( expression )
			let fn = tok
			guard let next = it.next(), next == "(" else { throw NSError(domain: "CalcError", code: 7, userInfo: [NSLocalizedDescriptionKey: "Error: Expected '(' after \(fn)"]) }
			var innerIt = it
			let arg = try evaluateExpression(&innerIt)
			it = innerIt
			let val = try applyFunction(fn, arg)
			nums.append(val)
			continue
		}

		if tok == "(" {
			// start sub-expression
			var innerIt = it
			let val = try evaluateExpression(&innerIt)
			it = innerIt
			nums.append(val)
			continue
		}

		if let first = tok.first, first.isNumber || (first == "-" && tok.count > 1) {
			if let v = Double(tok) { nums.append(v); continue }
			else { throw NSError(domain: "CalcError", code: 8, userInfo: [NSLocalizedDescriptionKey: "Error: Invalid number '\(tok)'!"]) }
		}

		// operator
		if tok.count == 1, let op = tok.first, precedence[op] != nil {
			while let top = ops.last {
				let p1 = precedence[op]!
				let p2 = precedence[top]!
				if (op == "^" && p1 < p2) || (op != "^" && p1 <= p2) {
					try applyTopOp()
				} else { break }
			}
			ops.append(op)
			continue
		}

		throw NSError(domain: "CalcError", code: 9, userInfo: [NSLocalizedDescriptionKey: "Error: Invalid token '\(tok)'"]) 
	}

	while !ops.isEmpty { try applyTopOp() }
	guard let res = nums.last else { throw NSError(domain: "CalcError", code: 10, userInfo: [NSLocalizedDescriptionKey: "Error: Empty expression!"]) }
	return res
}

func evaluate(_ tokens: [String]) throws -> Double {
	var it = tokens.makeIterator()
	return try evaluateExpression(&it)
}

// Write expression to ~/.calchistory
func writeHistory(_ expr: String) {
	guard !expr.isEmpty else { return }
	guard let home = ProcessInfo.processInfo.environment["HOME"] else { return }
	let path = NSString(string: home).appendingPathComponent(".calchistory")
	if FileManager.default.fileExists(atPath: path) {
		if let handle = FileHandle(forWritingAtPath: path) {
			handle.seekToEndOfFile()
			if let data = (expr + "\n").data(using: .utf8) { handle.write(data) }
			handle.closeFile()
		}
	} else {
		try? (expr + "\n").write(toFile: path, atomically: true, encoding: .utf8)
	}
}

func inlineMultipleMode() {
	while true {
		print("Enter expression: ", terminator: "")
		guard let line = readLine() else { break }
		let trimmed = line.trimmingCharacters(in: .whitespacesAndNewlines)
		if trimmed.isEmpty { continue }
		if trimmed.lowercased() == "exit" || trimmed.lowercased() == "quit" { break }

		writeHistory(trimmed)
		do {
			if !checkParentheses(trimmed) { throw NSError(domain: "CalcError", code: 11, userInfo: [NSLocalizedDescriptionKey: "Error: Mismatched parentheses."]) }
			let tokens = tokenize(trimmed)
			let result = try evaluate(tokens)
			print(result)
		} catch let e as NSError {
			fputs(e.localizedDescription + "\n", stderr)
		}
		print("------------------------")
		print("Continue? (y/n): ", terminator: "")
		if let c = readLine()?.trimmingCharacters(in: .whitespacesAndNewlines), (c == "n" || c == "N") { break }
	}
}

func inlineSingleMode() {
	print("Enter expression: ", terminator: "")
	guard let line = readLine() else { return }
	let trimmed = line.trimmingCharacters(in: .whitespacesAndNewlines)
	writeHistory(trimmed)
	do {
		if !checkParentheses(trimmed) { throw NSError(domain: "CalcError", code: 11, userInfo: [NSLocalizedDescriptionKey: "Error: Mismatched parentheses."]) }
		let tokens = tokenize(trimmed)
		let result = try evaluate(tokens)
		print(result)
	} catch let e as NSError {
		fputs(e.localizedDescription + "\n", stderr)
	}
}

func argumentMode(_ arg: String) {
	let trimmed = arg.trimmingCharacters(in: .whitespacesAndNewlines)
	writeHistory(trimmed)
	do {
		if !checkParentheses(trimmed) { throw NSError(domain: "CalcError", code: 11, userInfo: [NSLocalizedDescriptionKey: "Error: Mismatched parentheses."]) }
		let tokens = tokenize(trimmed)
		let result = try evaluate(tokens)
		print(result)
	} catch let e as NSError {
		fputs(e.localizedDescription + "\n", stderr)
	}
}

// MARK: - Main

@main
struct CalculatorApp {
	static func main() {
		let args = CommandLine.arguments
		var firstArg = ""
		if args.count > 1 { firstArg = args[1] }

		var mode: Mode = .inlineSingle
		var flagCount = 0
		for i in 1..<args.count {
			let a = args[i]
			if a == "-s" { mode = .inlineSingle; flagCount += 1 }
			else if a == "-m" { mode = .inlineMultiple; flagCount += 1 }
			else if a == "-a" { mode = .argument; flagCount += 1 }
		}

		if flagCount > 1 {
			fputs("Error: Only one flag may be provided at a time.\n", stderr)
			exit(1)
		}

		if flagCount == 0 && args.count > 1 {
			var oss = ""
			for i in 1..<args.count { if i > 1 { oss += " " }; oss += args[i] }
			argumentMode(oss)
			exit(0)
		}

		switch mode {
		case .inlineSingle:
			inlineSingleMode()
		case .inlineMultiple:
			inlineMultipleMode()
		case .argument:
			var oss = ""
			for i in 1..<args.count {
				let a = args[i]
				if !a.isEmpty && a.first == "-" { continue }
				if !oss.isEmpty { oss += " " }
				oss += a
			}
			if !oss.isEmpty { argumentMode(oss) }
			else if !firstArg.isEmpty { argumentMode(firstArg) }
			else { inlineSingleMode() }
		}
	}
}


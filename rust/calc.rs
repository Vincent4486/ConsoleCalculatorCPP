use std::env;
use std::io::{self, Write};
use std::fs::OpenOptions;
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

thread_local! {
    static CONTINUE_FLAG: Arc<AtomicBool> = Arc::new(AtomicBool::new(false));
}

enum Mode {
    Argument,
    InlineSingle,
    InlineMultiple
}

#[allow(dead_code)]
enum Modifier {
    S,
    M,
    A
}

fn apply_operation(a: f64, b: f64, op: char) -> Result<f64, String> {
    match op {
        '+' => Ok(a + b),
        '-' => Ok(a - b),
        '*' => Ok(a * b),
        '/' => {
            if b == 0.0 {
                Err("Error: Division by zero!".to_string())
            } else {
                Ok(a / b)
            }
        }
        '^' => Ok(a.powf(b)),
        _ => Err(format!("Error: Unknown operator '{}'!", op)),
    }
}

fn apply_function(fn_name: &str, x: f64) -> Result<f64, String> {
    match fn_name {
        "sqrt" => Ok(x.sqrt()),
        "log" | "ln" => {
            if x <= 0.0 {
                Err("Error: log domain error!".to_string())
            } else {
                Ok(x.ln())
            }
        }
        "sin" => Ok(x.sin()),
        "cos" => Ok(x.cos()),
        "tan" => Ok(x.tan()),
        _ => Err(format!("Error: Unknown function '{}'!", fn_name)),
    }
}

fn get_precedence(op: char) -> Option<i32> {
    match op {
        '+' | '-' => Some(1),
        '*' | '/' => Some(2),
        '^' => Some(3),
        _ => None,
    }
}

fn check_parentheses(s: &str) -> bool {
    let mut balance = 0;
    for ch in s.chars() {
        match ch {
            '(' => balance += 1,
            ')' => {
                balance -= 1;
                if balance < 0 {
                    return false;
                }
            }
            _ => {}
        }
    }
    balance == 0
}

fn tokenize(eq: &str) -> Vec<String> {
    let mut tokens = Vec::new();
    let mut cur = String::new();
    let chars: Vec<char> = eq.chars().collect();
    let mut i = 0;

    let flush = |tokens: &mut Vec<String>, cur: &mut String| {
        if !cur.is_empty() {
            tokens.push(cur.clone());
            cur.clear();
        }
    };

    while i < chars.len() {
        let ch = chars[i];

        if ch.is_whitespace() {
            i += 1;
            continue;
        }

        // Identifier (function name)
        if ch.is_alphabetic() {
            flush(&mut tokens, &mut cur);
            let mut name = String::new();
            while i < chars.len() && chars[i].is_alphabetic() {
                name.push(chars[i]);
                i += 1;
            }
            tokens.push(name);
            continue;
        }

        // Part of a number (digit, dot, or leading minus)
        if ch.is_numeric() || ch == '.' || (ch == '-' && (
            i == 0 ||
            chars[i - 1] == '(' ||
            get_precedence(chars[i - 1]).is_some()
        )) {
            cur.push(ch);
            i += 1;
        } else {
            flush(&mut tokens, &mut cur);
            tokens.push(ch.to_string());
            i += 1;
        }
    }
    flush(&mut tokens, &mut cur);
    tokens
}

fn evaluate_expression(tokens: &[String], idx: &mut usize) -> Result<f64, String> {
    let mut nums: Vec<f64> = Vec::new();
    let mut ops: Vec<char> = Vec::new();

    while *idx < tokens.len() && tokens[*idx] != ")" {
        let tok = &tokens[*idx];

        // Function call
        if tok.chars().next().map_or(false, |c| c.is_alphabetic()) {
            let fn_name = tok.clone();
            *idx += 1;
            if *idx >= tokens.len() || tokens[*idx] != "(" {
                return Err(format!("Error: Expected '(' after {}", fn_name));
            }
            *idx += 1;
            let arg = evaluate_expression(tokens, idx)?;
            nums.push(apply_function(&fn_name, arg)?);
            continue;
        }

        // Sub-expression
        if tok == "(" {
            *idx += 1;
            nums.push(evaluate_expression(tokens, idx)?);
            continue;
        }

        // Number literal
        if tok.chars().next().map_or(false, |c| c.is_numeric() || (c == '-' && tok.len() > 1)) {
            nums.push(tok.parse::<f64>().map_err(|_| format!("Error: Invalid number '{}'", tok))?);
            *idx += 1;
            continue;
        }

        // Operator
        if tok.len() == 1 {
            if let Some(op_char) = tok.chars().next() {
                if let Some(p1) = get_precedence(op_char) {
                    while let Some(&top_op) = ops.last() {
                        if let Some(p2) = get_precedence(top_op) {
                            let should_pop = if op_char == '^' {
                                p1 < p2
                            } else {
                                p1 <= p2
                            };
                            if should_pop {
                                ops.pop();
                                let b = nums.pop().ok_or("Error: Invalid expression")?;
                                let a = nums.pop().ok_or("Error: Invalid expression")?;
                                nums.push(apply_operation(a, b, top_op)?);
                            } else {
                                break;
                            }
                        } else {
                            break;
                        }
                    }
                    ops.push(op_char);
                    *idx += 1;
                    continue;
                }
            }
        }

        return Err(format!("Error: Invalid token '{}'", tok));
    }

    // Consume ')'
    if *idx < tokens.len() && tokens[*idx] == ")" {
        *idx += 1;
    }

    // Flush remaining ops
    while let Some(op) = ops.pop() {
        let b = nums.pop().ok_or("Error: Invalid expression")?;
        let a = nums.pop().ok_or("Error: Invalid expression")?;
        nums.push(apply_operation(a, b, op)?);
    }

    if nums.is_empty() {
        return Err("Error: Empty expression!".to_string());
    }
    Ok(nums[0])
}

fn evaluate(tokens: &[String]) -> Result<f64, String> {
    let mut idx = 0;
    evaluate_expression(tokens, &mut idx)
}

fn write_history(expr: &str) {
    if expr.is_empty() {
        return;
    }
    if let Ok(home) = env::var("HOME") {
        let path = PathBuf::from(home).join(".calchistory");
        if let Ok(mut file) = OpenOptions::new().create(true).append(true).open(&path) {
            let _ = writeln!(file, "{}", expr);
        }
    }
}

fn ask_continue() {
    print!("Continue? (Y/n): ");
    let _ = io::stdout().flush();
    let mut input = String::new();
    let _ = io::stdin().read_line(&mut input);
    let response = input.trim();
    if response == "n" || response == "N" {
        CONTINUE_FLAG.with(|flag| flag.store(true, Ordering::Relaxed));
        println!("Exiting...");
    }
}

fn argument_mode(args: &str) {
    write_history(args);
    match check_parentheses(args) {
        false => eprintln!("Error: Mismatched parentheses."),
        true => {
            let tokens = tokenize(args);
            match evaluate(&tokens) {
                Ok(result) => println!("{}", result),
                Err(e) => eprintln!("{}", e),
            }
        }
    }
}

fn inline_single_mode() {
    let mut line = String::new();
    print!("Enter expression: ");
    let _ = io::stdout().flush();
    if io::stdin().read_line(&mut line).is_ok() {
        let line = line.trim();
        write_history(line);
        match check_parentheses(line) {
            false => eprintln!("Error: Mismatched parentheses."),
            true => {
                let tokens = tokenize(line);
                match evaluate(&tokens) {
                    Ok(result) => println!("{}", result),
                    Err(e) => eprintln!("{}", e),
                }
            }
        }
    }
}

fn inline_multiple_mode() {
    loop {
        let should_exit = CONTINUE_FLAG.with(|flag| flag.load(Ordering::Relaxed));
        if should_exit {
            break;
        }
        let mut line = String::new();
        print!("Enter expression: ");
        let _ = io::stdout().flush();
        if io::stdin().read_line(&mut line).is_err() {
            break;
        }
        let line = line.trim();
        write_history(line);
        match check_parentheses(line) {
            false => eprintln!("Error: Mismatched parentheses."),
            true => {
                let tokens = tokenize(line);
                match evaluate(&tokens) {
                    Ok(result) => println!("{}", result),
                    Err(e) => eprintln!("{}", e),
                }
            }
        }
        println!("------------------------");
        ask_continue();
    }
}

fn main() {
    let argv: Vec<String> = env::args().collect();
    let argc = argv.len();

    let first_arg = if argc > 1 {
        argv[1].clone()
    } else {
        String::new()
    };

    let mut mode: Mode = Mode::InlineSingle;

    let _saw_set_var: bool = false;
    let _saw_clear_var: bool = false;
    let mut flag_count: i32 = 0;
    for i in 1..argc {
        let a: String = argv[i].clone();
        if a == "-s" {
            mode = Mode::InlineSingle;
            flag_count += 1;
            continue;
        }
        if a == "-m" {
            mode = Mode::InlineMultiple;
            flag_count += 1;
            continue;
        }
        if a == "-a" {
            mode = Mode::Argument;
            flag_count += 1;
            continue;
        }
    }
    
    if flag_count > 1 {
        eprintln!("Error: Multiple mode flags provided.");
        std::process::exit(1);
    }

    if flag_count == 0 && argc > 1 {
        let mut args = Vec::new();
        for i in 1..argc {
            args.push(argv[i].clone());
        }
        let combined = args.join(" ");
        argument_mode(&combined);
        return;
    }

    match mode {
        Mode::InlineSingle => inline_single_mode(),
        Mode::InlineMultiple => inline_multiple_mode(),
        Mode::Argument => {
            // Build expression from non-flag argv elements (ignore known flags)
            let mut args = Vec::new();
            for i in 1..argc {
                let a = &argv[i];
                if !a.is_empty() && a.starts_with('-') {
                    continue; // skip flags
                }
                args.push(a.clone());
            }
            
            if !args.is_empty() {
                let combined = args.join(" ");
                argument_mode(&combined);
            } else if !first_arg.is_empty() {
                argument_mode(&first_arg);
            } else {
                // No argument provided; fall back to inline single prompt
                inline_single_mode();
            }
        }
    }
}

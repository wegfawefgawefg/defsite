use notify::{Config, RecommendedWatcher, RecursiveMode, Watcher};
use regex::Regex;
use std::collections::HashSet;
use std::fs::{self, File};
use std::io::{BufRead, BufReader, Write};
use std::path::{Path, PathBuf};
use std::sync::mpsc::channel;
use std::time::Duration;

fn main() -> std::io::Result<()> {
    let src_dir = Path::new("./src");
    let rendered_dir = Path::new("./generated");
    fs::create_dir_all(rendered_dir)?;

    let args: Vec<String> = std::env::args().collect();
    let watch_mode = args.contains(&"--watch".to_string());

    if watch_mode {
        println!("Running in watch mode. Press Ctrl+C to stop.");
        watch_and_generate(src_dir, rendered_dir)?;
    } else {
        generate_site(src_dir, rendered_dir)?;
    }

    Ok(())
}

fn watch_and_generate(src_dir: &Path, rendered_dir: &Path) -> std::io::Result<()> {
    let (tx, rx) = channel();

    let mut watcher = match RecommendedWatcher::new(tx, Config::default()) {
        Ok(w) => w,
        Err(e) => return Err(std::io::Error::new(std::io::ErrorKind::Other, e)),
    };

    if let Err(e) = watcher.watch(src_dir, RecursiveMode::Recursive) {
        return Err(std::io::Error::new(std::io::ErrorKind::Other, e));
    }

    let mut last_generation = std::time::Instant::now();
    let debounce_duration = Duration::from_millis(100);

    loop {
        match rx.recv() {
            Ok(event) => {
                println!("Change detected: {:?}", event);
                if last_generation.elapsed() > debounce_duration {
                    if let Err(e) = generate_site(src_dir, rendered_dir) {
                        eprintln!("Error generating site: {:?}", e);
                    }
                    last_generation = std::time::Instant::now();
                }
            }
            Err(e) => println!("Watch error: {:?}", e),
        }
    }
}
fn generate_site(src_dir: &Path, rendered_dir: &Path) -> std::io::Result<()> {
    let mut errors = Vec::new();
    let mut processed_files = HashSet::new();
    process_directory(src_dir, rendered_dir, &mut errors, &mut processed_files)?;

    if errors.is_empty() {
        println!("\x1b[32mStatic site generation complete.\x1b[0m");
    } else {
        println!("\x1b[31mStatic site generation completed with errors:\x1b[0m");
        for error in &errors {
            println!("- {}", error);
        }
        println!("\x1b[31mGeneration failed due to errors.\x1b[0m");
        println!("\x1b[33mFix it and run again :^)\x1b[0m");
    }

    Ok(())
}

fn process_directory(
    src: &Path,
    dest: &Path,
    errors: &mut Vec<String>,
    processed_files: &mut HashSet<PathBuf>,
) -> std::io::Result<()> {
    for entry in fs::read_dir(src)? {
        let entry = entry?;
        let path = entry.path();
        let dest_path = dest.join(path.strip_prefix(src).unwrap());

        if path.is_dir() {
            fs::create_dir_all(&dest_path)?;
            process_directory(&path, &dest_path, errors, processed_files)?;
        } else {
            if !processed_files.contains(&path) {
                if path.extension().and_then(|s| s.to_str()) == Some("html") {
                    if let Err(e) = process_html_file(&path, &dest_path, errors, processed_files) {
                        errors.push(format!("Error processing {:?}: {}", path, e));
                    }
                } else {
                    if let Err(e) = fs::copy(&path, &dest_path) {
                        errors.push(format!(
                            "Error copying {:?} to {:?}: {}",
                            path, dest_path, e
                        ));
                    }
                }
                processed_files.insert(path.clone());
                println!("Processed: {:?} -> {:?}", path, dest_path);
            }
        }
    }
    Ok(())
}

fn process_html_file(
    input_path: &Path,
    output_path: &Path,
    errors: &mut Vec<String>,
    processed_files: &mut HashSet<PathBuf>,
) -> std::io::Result<()> {
    let input_file = File::open(input_path)?;
    let reader = BufReader::new(input_file);
    let mut output_file = File::create(output_path)?;

    let template_regex = Regex::new(r"<!-- template: (.+?) -->").unwrap();

    for (line_number, line) in reader.lines().enumerate() {
        let line = line?;
        if let Some(captures) = template_regex.captures(&line) {
            let template_name = captures.get(1).unwrap().as_str();
            let template_path = input_path.parent().unwrap().join(template_name);
            if template_path.exists() {
                let template_content = fs::read_to_string(&template_path)?;
                writeln!(output_file, "{}", template_content)?;
                processed_files.insert(template_path);
            } else {
                let error_msg = format!(
                    "\x1b[31mWarning: Template {} not found for {:?}:{}\x1b[0m",
                    template_name,
                    input_path,
                    line_number + 1 // Adding 1 because line numbers are typically 1-indexed
                );
                errors.push(error_msg.clone());
                eprintln!("{}", error_msg);
                writeln!(output_file, "{}", line)?;
            }
        } else {
            writeln!(output_file, "{}", line)?;
        }
    }
    Ok(())
}

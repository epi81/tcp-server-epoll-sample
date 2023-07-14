use std::env;
use std::io::{self, Read, Write};
use std::net::TcpStream;
use std::process::id;

fn establish_connection(ip: &str, port: u16) -> io::Result<TcpStream> {
    let stream = TcpStream::connect(format!("{}:{}", ip, port))?;
    Ok(stream)
}

fn client(stream: &mut TcpStream) -> io::Result<()> {
    let pid = id().to_string();

    for counter in 1..=10 {
        let counter_str = counter.to_string();
        let message = format!("{}:{}", pid, counter_str);
        stream.write_all(message.as_bytes())?;

        let mut buffer = [0; 1024];
        let bytes_read = stream.read(&mut buffer)?;
        let response = String::from_utf8_lossy(&buffer[..bytes_read]);
        println!("{}",response);
    }
    Ok(())
}

fn usage() {
    eprintln!("usage: echo-client [IP] [Port]");
}

fn main() {
    let args: Vec<String> = env::args().collect();

    if args.len() > 3 {
        usage();
        return;
    }
    
    let ip = args.get(1).map(|ip| ip.as_str()).unwrap_or("127.0.0.1");
    let port: u16 = args.get(2).and_then(|port| port.parse().ok()).unwrap_or(1234);

    match establish_connection(ip, port) {
        Ok(mut stream) => {
            if let Err(err) = client(&mut stream) {
                eprintln!("client error: {}", err);
            }
        }
        Err(err) => {
            eprintln!("{}: {}", ip, err);
        }
    }
}

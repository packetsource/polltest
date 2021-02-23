package main
import "net"
import "io"
import "flag"
import "bytes"
import "log"
import "time"

func main() {
	port := flag.String("p", "2020", "TCP port")
	size := flag.Int("b", 1024, "number of bytes to serve")
	wait := flag.Int("w", 0, "number of seconds to wait before closing after serving data")

	flag.Parse()

	buffer := make ([]byte, *size, *size)

	listener, err := net.Listen("tcp", ":"+*port)
	if err != nil {
		log.Fatal(err)
	}
	defer listener.Close()

	for {
		conn, err := listener.Accept()
		if err != nil {
			log.Println(err)
			continue
		} else {
			log.Printf("Serving new client %s\n", conn.RemoteAddr())
			go func(conn net.Conn) {
				io.Copy(conn, bytes.NewReader(buffer))
				if *wait > 0 {
					time.Sleep(1*time.Second)
				}
				conn.Close()
			}(conn)
		}
	}
}

package main

import (
	"fmt"
	"mime"
	"net/http"

	"github.com/nsf/termbox-go"
)

func main() {
	contentType := mime.TypeByExtension(".json")

	err := termbox.Init()
	if err != nil {
		panic(err)
	}
	defer termbox.Close()

	for {
		switch ev := termbox.PollEvent(); ev.Type {
		case termbox.EventKey:
			switch ev.Key {
			case termbox.KeyEsc:
				fmt.Println("end")
				return
			case termbox.KeyArrowUp:
				go http.Post("http://192.168.1.112:4000/v1/engine/fadein", contentType, nil)
			case termbox.KeyArrowDown:
				go http.Post("http://192.168.1.112:4000/v1/engine/fadeout", contentType, nil)
			case termbox.KeyArrowLeft:
				go http.Post("http://192.168.1.112:4000/v1/engine/start", contentType, nil)
			case termbox.KeyArrowRight:
				go http.Post("http://192.168.1.112:4000/v1/engine/quit", contentType, nil)
			case termbox.KeySpace:
			default:
				go http.Post("http://192.168.1.112:4000/v1/engine/feed", contentType, nil)
				go http.Post("http://192.168.1.112:4000/sound/feed", contentType, nil)
			}
		}
	}
}

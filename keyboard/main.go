package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"log"
	"mime"
	"net/http"

	"github.com/nsf/termbox-go"
)

type command struct {
	Type  int         `json:"type"`
	Value interface{} `json:"value"`
}

type commandRequest struct {
	Commands []command `json:"commands"`
}

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
				go http.Get("http://192.168.1.107:4000/v1/audio/restart")
			case termbox.KeyArrowDown:
				go http.Get("http://192.168.1.107:4000/v1/audio/pause")
			case termbox.KeyArrowLeft:
				go http.Get("http://192.168.1.107:4000/v1/audio/enable")
			case termbox.KeyArrowRight:
				go http.Get("http://192.168.1.107:4000/v1/audio/disable")
			case termbox.KeySpace:
			default:
				if ev.Ch == []rune("j")[0] {
					go http.Post("http://192.168.1.107:4000/v1/voice/pitch?diff=-0.05", contentType, nil)
				}
				if ev.Ch == []rune("k")[0] {
					go http.Post("http://192.168.1.107:4000/v1/voice/pitch?diff=0.05", contentType, nil)
				}
				if ev.Ch == []rune("h")[0] {
					go http.Post("http://192.168.1.107:4000/v1/voice/rate?diff=-0.05", contentType, nil)
				}
				if ev.Ch == []rune("l")[0] {
					go http.Post("http://192.168.1.107:4000/v1/voice/rate?diff=0.05", contentType, nil)
				}
				cmd := commandRequest{
					Commands: []command{
						{
							Type:  1,
							Value: 0,
						},
						{
							Type:  3,
							Value: "Hello, 世界",
						},
					},
				}
				data, err := json.Marshal(cmd)
				if err != nil {
					log.Fatal(err)
				}
				if ev.Ch == []rune("1")[0] {
					go http.Post("http://192.168.1.107:4000/v1/audio/command", contentType, bytes.NewBuffer(data))
				} else {
					go http.Post("http://192.168.1.107:4000/v1/audio/command?force=true", contentType, bytes.NewBuffer(data))
				}
			}
		}
	}
}

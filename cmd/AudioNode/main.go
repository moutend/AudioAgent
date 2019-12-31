package main

import (
	"log"
	"os"
	"os/signal"

	"github.com/moutend/AudioNode/pkg/app"
)

func main() {
	if err := run(os.Args); err != nil {
		log.New(os.Stderr, "", 0).Fatal(err)
	}
}

func run(args []string) error {
	app := app.New()

	if err := app.Setup(); err != nil {
		return err
	}

	interrupt := make(chan os.Signal, 1)
	signal.Notify(interrupt, os.Interrupt)

	<-interrupt

	if err := app.Teardown(); err != nil {
		return err
	}

	return nil
}

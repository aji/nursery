// container.rs -- theater actor containers
// Copyright (C) 2015 Alex Iadicicco

// The stage, if you will.

//! Actors live inside of a container. Containers are confined to a single
//! thread and have immediate address space access to the actors they manage.
//! Messages addressed to an actor residing in a container are placed in that
//! container's input queue, where they are eventually delivered to the actor.

use std::sync::mpsc;
use std::sync::{Arc, Mutex};

use rustc_serialize::Json;

use actor::Actor;

pub struct Container {
    actors: HashMap<u32, Box<Actor>>

    message_rx: mpsc::Receiver<Envelope>,
    message_tx: mpsc::Sender<Envelope>,

    peers: Arc<Mutex<Peers>>
}

impl Container {
    fn run(&mut self) {
        for envelope in self.message_rx {
            match self.actor_for(envelope.address) {
                Some(x) => x.deliver(envelope.message),
                None => { continue }
            }
        }
    }
}

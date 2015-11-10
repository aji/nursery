// actor.rs -- theater actor definitions and helpers
// Copyright (C) 2015 Alex Iadicicco

//! Actors form the core of the actor model. This module provides the
//! definitions used throughout the project for describing and managing actors.

use rustc_serialize;
use rustc_serialize::{Encodable, Decodable};

/// Actor behavior is defined by an actor definition, which is any type
/// implementing the `Actor` trait.
pub trait Actor {
    /// The type of messages this actor expects to receive. Incoming messages
    /// are automatically decoded to this type. Similarly, messages sent to this
    /// actor are encoded using this type. Decode or encode failures are
    /// considered a kind of message delivery failure.
    type Message: Encodable + Decodable;

    /// The heart of the actor, where messages are processed internally as state
    /// changes and produce zero or more actions.
    fn recv(&mut self, m: Self::Message) -> Actions;
}

trait ActorIntrinsics: Actor {
    fn deliver<D>(&mut self, d: &mut D)
    where D: rustc_serialize::Decoder {
        match Self::Message::decode(d) {
            Ok(m) => self.recv(m),
            Err(_) => { }
        }
    }
}

impl<A: Actor> ActorIntrinsics for A { }

/// A reference to an actor instance. Contains information about the location
/// of the actor.
#[derive(Copy, Clone)]
pub enum ActorRef {
    Local(LocalActorRef),
    Remote(RemoteActorRef),
}

/// A reference to an actor running on the current host. Messages to these
/// instances can be sent relatively quickly, and are less likely to be lost.
#[derive(Copy, Clone)]
pub struct LocalActorRef {
    thread: u32,
    ident: u32,
}

/// A reference to an actor running on a remote host. Messages to these
/// instances are much slower and more error prone to deliver.
#[derive(Copy, Clone)]
pub struct RemoteActorRef;

#[cfg(test)]
mod tests {
    use actor::Actor;
    use actor::ActorIntrinsics;

    #[derive(RustcEncodable, RustcDecodable)]
    struct DumyMessage(i32);

    struct DumyActor {
        value: i32
    }

    impl DumyActor {
        fn new_with_value(val: i32) -> DumyActor {
            DumyActor { value: val }
        }
    }

    impl Actor for DumyActor {
        type Message = DumyMessage;

        fn recv(&mut self, DumyMessage(val): DumyMessage) {
            self.value = val;
        }
    }

    #[test]
    fn test_encode_decode_deliver_works() {
        use rustc_serialize::json;

        let mut actor = DumyActor::new_with_value(5);
        let encoded = match json::encode(&DumyMessage(7)) {
            Ok(v) => v,
            Err(_) => panic!("encode failed")
        };
        let parsed = match json::Json::from_str(&encoded[..]) {
            Ok(v) => v,
            Err(_) => panic!("parse failed")
        };

        actor.deliver(&mut json::Decoder::new(parsed));

        assert_eq!(actor.value, 7);
    }

    #[test]
    fn test_delivery_failure_on_parse() {
        use rustc_serialize::json;

        let mut actor = DumyActor::new_with_value(5);
        let parsed = match json::Json::from_str("[]") {
            Ok(v) => v,
            Err(_) => panic!("parse failed")
        };

        actor.deliver(&mut json::Decoder::new(parsed));

        assert_eq!(actor.value, 5);

        // TODO: finish this test
    }
}

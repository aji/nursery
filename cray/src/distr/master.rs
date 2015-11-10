// master.rs -- master renderer
// Copyright (C) 2015 Alex Iadicicco

//! Master renderer

use std::sync::Arc;
use std::sync::Mutex;
use std::sync::mpsc;

use super::WorkItem;
use super::WorkResult;

pub struct Master {
    total_items: usize,

    req_tx: mpsc::Sender<WorkItem>,
    req_rx: mpsc::Receiver<WorkItem>,

    res_tx: mpsc::Sender<WorkResult>,
    res_rx: mpsc::Receiver<WorkResult>,
}

impl Master {
    pub fn new(items: Vec<WorkItem>) -> Master {
        let (req_tx, req_rx) = mpsc::channel();
        let (res_tx, res_rx) = mpsc::channel();

        for item in items.iter() {
            req_tx.send(item).unwrap();
        }

        Master {
            total_items: items.len(),

            req_tx: req_tx,
            req_rx: req_rx,

            res_tx: res_tx,
            res_rx: res_rx,
        }
    }
}

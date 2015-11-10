proc OnClient { ch addr port } {
    fileevent $ch readable {
    }
}

socket -server OnClient 6667
vwait forever

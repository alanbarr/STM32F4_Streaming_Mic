# Not RTP

This is not a full fledged RTP protocol stack.
The RTP protocol was chosen to prevent the need for a bespoke protocol to stream
audio data.
It was hoped that limited RTP functionality could be cobbled together to premit
some basic interoperability with other devices.

# Assumptions
* Point to point between 2 devices (only ever 1 remote device, max).
* *May* be bi-directional. Not yet supported but kind-of, maybe thought about.
* Remote sockets for reception are the same as transmission.
* Symmetric RTP ?

# pipes
OS and System Programming, lab #7

<img src="https://github.com/NRGb3nder/pipes/blob/master/scheme.png" alt="Scheme" width="75%">

Implement process interaction model according to the above scheme. It is required to use pipes to create worker-worker and controller-worker data exchange streams. A controller process should terminate all workers after interaction.

My interpretation of this scheme:

<i>The controller process sends a poll request to each of the worker processes. After receiving the poll request the worker process sends another kind of request to its colleagues and awaits for confirmation. When confirmation is sent by receiver and received by request suppliant, a confirmation counter is incremented. It should reach number of workers - 1 to make its owner be ready to send confirmation back to the controller process. The procedure is repeated until there is no unpolled workers.</i>

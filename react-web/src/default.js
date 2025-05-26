let socket = new WebSocket("ws://" + document.location.hostname + ":" + document.location.port + "/socket");

export const resetSwitches = () => {
    let count = 0;
    for (let i = 0; i < 32; ++i) {
        const id = "a" + i;
        const cb = document.getElementById(id);
        if (cb != undefined) {
            ++count;
        } else {
            break;
        }
    }
    const buf = new ArrayBuffer(5);
    const view = new DataView(buf,0);
    view.setUint8(0,count);
    let packed = 0;
    
    view.setUint32(1,0,false);
    socket.send(buf);    
}
export const setSwitches = () => {
    const values = []
    for (let i = 0; i < 32; ++i) {
        const id = "a" + i;
        const cb = document.getElementById(id);
        if (cb != undefined) {
            values.push(cb.checked);
        } else {
            break;
        }
    }
    const buf = new ArrayBuffer(5);
    const view = new DataView(buf,0);
    view.setUint8(0,values.length);
    let packed = 0;
    for(let i = values.length-1;i>=0;--i) {
        packed <<= 1;
        if(values[i]) {
            packed|=1;
        }
    }
    view.setUint32(1,packed,false);
    socket.send(buf);
}
const reconnectSwitches = () => {
    socket = new WebSocket("ws://" + document.location.hostname + ":" + document.location.port + "/socket");
}
export const connectSwitches = () => {
    socket.binaryType = "arraybuffer";
    // Connection opened
    socket.addEventListener("open", event => {
        console.log("connected");
        // Listen for messages
        socket.addEventListener("message", event => {
            let res = [];
            if (event.data instanceof ArrayBuffer) {
                const view = new DataView(event.data);
                if (view.byteLength != 5) {
                    console.log("Byte length of " + view.byteLength + " unexpected");
                } else {
                    const count = view.getUint8(0);
                    let data = view.getUint32(1, false);
                    for (let i = 0; i < count; ++i) {
                        if (((data >>> i) % 2) != 0) {
                            res.push(true);
                        } else {
                            res.push(false);
                        }
                    }
                    for (let i = 0; i < count; ++i) {
                        const id = "a" + i;
                        const cb = document.getElementById(id);
                        if (cb != undefined) {
                            cb.checked = res[i];
                        }
                    }
                }
            }
        });
        socket.addEventListener("close", (event) => { 
            setTimeout(reconnectSwitches,100);
        });
        // rerequest the data
        const data = new ArrayBuffer(1);
        const view = new DataView(data);
        view.setUint8(0);
        // the actual data sent is ignored by the server
        socket.send(data);
        console.log("send refresh request");
    });

    
}
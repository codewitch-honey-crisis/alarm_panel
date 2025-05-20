import { useEffect, Suspense } from "react";
import './App.css'
import { connectSwitches, setSwitches, resetSwitches } from './default.js';
const fetchData = () => {
    let data;
    const promise = fetch("./api/")
        .then((response) => response.json())
        .then((json) => (data = json));
    return {
        read() {
            if (!data) {
                throw promise;
            }
            return data;
        },
    };
};
const alarmData = fetchData();
function Alarm({ index, value }) {
    return (
        <>
            <label className="switchLabel">{index + 1}</label>
            <label className="switch">
                <input id={"a" + index} type="checkbox" name="a" defaultChecked={value} onClick={setSwitches} value={index} />
                <span className="slider round"></span>
            </label>
            <span>&nbsp;&nbsp;</span>
        </>
    )
}
const Alarms = () => {
    const status = alarmData.read();
    const alarms = [];
    for (let i: number = 0; i < status.length; ++i) {
        alarms.push(<Alarm index={i} value={status[i]} />);
    }
    return (<form method="get" action="#">{alarms}</form>)

};
export default function App() {
    useEffect(() => {
        connectSwitches();
    }, []);
    return (
        <Suspense fallback={<p>waiting for data...</p>}>
            <>
                <h1>Alarm Control Panel</h1>
                <Alarms />
                <br />
                <button className="button" type="button" onClick={resetSwitches}>Reset All</button>
            </>
        </Suspense>
    );
}

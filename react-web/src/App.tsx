import { useEffect, Suspense } from "react";
import './App.css'
import { switchValues, connectSwitches, setSwitches, resetSwitches } from './default.js';
function Alarm({ index, value }) {
    return (
        <>
            <label className="switchLabel">{index + 1}</label>
            <label className="switch">
                <input id={"a" + index} type="checkbox" name="a" defaultChecked={value} onClick={setSwitches} value={index} />
                <span className="slider round"></span>
            </label>
            <span>  </span>
        </>
    )
}
const Alarms = () => {
    const alarms = switchValues;
    for (let i: number = 0; i < alarms.length; ++i) {
        alarms.push(<Alarm index={i} value={alarms[i]} />);
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

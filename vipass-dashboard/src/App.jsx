import { Route, Routes } from "react-router-dom";

import Home from "./pages/Home";
import PageWrapper from "./layout/PageWrapper"
import Sidebar from "./layout/Sidebar"
import CarRegister from "./pages/CarRegister";
import Information from "./pages/Information";
import { useEffect } from "react";
import { WebSocketProvider } from "./context/WebSocketProvider";

function App() {

  useEffect(() => {

  }, []);

  return (
    <WebSocketProvider>
      <PageWrapper>
        <Sidebar />
        <Routes>
          <Route path="/" element={<Home />} />
          <Route path="/info" element={<Information />} />
          <Route path="/register/carinfo" element={<CarRegister />} />
        </Routes>
      </PageWrapper>
    </WebSocketProvider>
  )
}

export default App;
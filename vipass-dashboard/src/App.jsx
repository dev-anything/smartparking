import Content from "./layout/ContentWrapper"
import PageWrapper from "./layout/PageWrapper"
import Sidebar from "./layout/Sidebar"

function App() {

  return (
    <PageWrapper>
      <Sidebar />
      <Content />
    </PageWrapper>
  )
}

export default App

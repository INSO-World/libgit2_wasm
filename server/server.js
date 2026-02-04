import express from "express";

const app = express();
const PORT = 8080;

app.use((req, res, next) => {
    res.setHeader("Cross-Origin-Opener-Policy", "same-origin");
    res.setHeader("Cross-Origin-Embedder-Policy", "require-corp");
    next();
});
app.use(express.static("src"));

app.listen(PORT, () => {
    console.log(`Server running at http://localhost:${PORT}`);
});

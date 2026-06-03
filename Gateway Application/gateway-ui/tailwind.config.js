/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}", // <-- This tells Tailwind to scan all your React/TypeScript files
  ],
  theme: {
    extend: {},
  },
  plugins: [],
}

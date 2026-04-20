FROM gcc:13
WORKDIR /app
COPY . .
RUN apt-get update && apt-get install -y nodejs npm
RUN g++ -std=c++17 -O2 -o fitness_backend fitness_backend.cpp
RUN npm install
EXPOSE 3000
CMD ["npm", "start"]

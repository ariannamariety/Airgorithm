// Arianna Ty

#include <SFML/Graphics.hpp>
#include "graph.h"
#include <chrono>

sf::Vector2f projectCoords(double lat, double lon, int width, int height) {
  float x=  (lon + 180.f) * (width / 360.f);
  float y = (90.f - lat) * (height / 180.f);
  return {x,y};
}

struct InputBox {
  sf::RectangleShape box;
  sf::Text text;
  bool active = false;
  std::string input;

  InputBox(float x, float y, float w, float h, const sf::Font& font) {
    box.setPosition(x, y);
    box.setSize({w,h});
    box.setFillColor(sf::Color::White);
    box.setOutlineThickness(2);
    box.setOutlineColor(sf::Color(100,100,100));

    text.setFont(font);
    text.setCharacterSize(22);
    text.setFillColor(sf::Color::Black);
    text.setPosition(x+10,y+5);
  }

  void handleEvent(const sf::Event& e) {
    if (e.type == sf::Event::MouseButtonPressed) {
      sf::Vector2f mouse(e.mouseButton.x, e.mouseButton.y);
      active = box.getGlobalBounds().contains(mouse);
      box.setOutlineColor(active ? sf::Color::Blue : sf::Color(100, 100, 100));
    }

    if (active && e.type == sf::Event::TextEntered) {
      if (e.text.unicode == 8 && !input.empty()) // backspace
        input.pop_back();
      else if (e.text.unicode < 128 && isalnum(e.text.unicode))
        input.push_back(static_cast<char>(toupper(e.text.unicode)));

      text.setString(input);
    }
  }

  std::string getValue() const {
    return input;
  }
};

int main() {
  const int WIDTH = 1200, HEIGHT = 800;

  sf::RenderWindow window (sf::VideoMode(WIDTH, HEIGHT), "Airgorithm - Flight Map Visualizer");
  window.setFramerateLimit(60);


  // load font
  sf::Font font;
  if (!font.loadFromFile("C:/Windows/Fonts/arial.ttf")) {
    std::cerr << "Failed to load font" << std::endl;
    return 1;
  }

  // load dataset
  FlightGraph G;
  if (!G.loadAirportsDat("data/airports.dat")) {
    std::cerr << "Error loading airports.dat\n";
    return 1;
  }
  if (!G.loadFromEstimatedCSV("data/routes_with_estimated_times_plus_33k.csv")) {
    std::cerr << "Failed to load csv data" << std::endl;
    return 1;
  }

  sf::RectangleShape panel(sf::Vector2f(WIDTH, 200));
  panel.setPosition(0, 600);
  panel.setFillColor(sf::Color(245, 245, 245));

  InputBox srcBox(180, 630, 200, 35, font);
  InputBox dstBox(600, 630, 200, 35, font);

  // labels
  sf::Text srcLabel("Source Airport:", font, 20);
  sf::Text dstLabel("Destination Airport:", font, 20);
  srcLabel.setFillColor(sf::Color::Black);
  dstLabel.setFillColor(sf::Color::Black);
  srcLabel.setPosition(40, 637);
  dstLabel.setPosition(420, 637);

  // run button
  sf::RectangleShape runBtn({200, 40});
  runBtn.setPosition(900, 625);
  runBtn.setFillColor(sf::Color(70, 130, 180));

  sf::Text runText("Run Algorithms", font, 20);
  runText.setPosition(920, 625);
  runText.setFillColor(sf::Color::White);

  // output text
  sf::Text outputText("", font, 18);
  outputText.setPosition(100,680);
  outputText.setFillColor(sf::Color::Black);

  bool hasResult = false;

  while (window.isOpen()) {
    sf::Event e;
    while (window.pollEvent(e)) {
      if (e.type == sf::Event::Closed)
        window.close();

      srcBox.handleEvent(e);
      dstBox.handleEvent(e);

      if (e.type == sf::Event::MouseButtonPressed) {
        sf::Vector2f mouse(e.mouseButton.x, e.mouseButton.y);
        if(runBtn.getGlobalBounds().contains(mouse)) {
          std::string src = srcBox.getValue();
          std::string dst = dstBox.getValue();

          if(!src.empty() && !dst.empty()) {
            auto start1 = std::chrono::high_resolution_clock::now();
            auto dijkstraRes = G.dijkstra(src, dst);
            auto end1 = std::chrono::high_resolution_clock::now();

            auto start2 = std::chrono::high_resolution_clock::now();
            auto bellmanRes = G.bellmanFord(src, dst);
            auto end2 = std::chrono::high_resolution_clock::now();

            long long dTime = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1).count();
            long long bTime = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2).count();

            if (dijkstraRes.second.empty() || bellmanRes.second.empty()) {
              outputText.setString("No valid route found!");
            } else {
              std::ostringstream oss;
              oss << std::fixed << std::setprecision(2);
              oss << "Dijkstra: " << src << " -> " << dst
                  << " | Time: " << dijkstraRes.first << " hrs"
                  << " | Stops: " << (dijkstraRes.second.size() - 1)
                  << " | Took: " << dTime << " ms/n | ";
              oss << "Bellman-Ford: " << src << " -> " << dst
                  << " | Time: " << bellmanRes.first << " hrs"
                  << " | Stops: " << (bellmanRes.second.size() - 1)
                  << " | Took: " << bTime << " ms/n";

              if ( dijkstraRes.first == bellmanRes.first)
                oss << "\n Both Algorithms found the same shortest route.";
              else
                oss << "\n Routes differ!";

              outputText.setString(oss.str());
            }

            hasResult = true;
          } else {
            outputText.setString("Please enter both airport codes!");
            hasResult = true;
          }
        }
      }
    }

    window.clear(sf::Color::White);

    for (const auto& airport : G.airports) {
      sf::Vector2f pos = projectCoords(airport.latitude, airport.longitude, WIDTH, 600);
      sf::CircleShape node(3);
      node.setFillColor(sf::Color::Black);
      node.setOrigin(3,3);
      node.setPosition(pos);
      window.draw(node);
    }

    window.draw(panel);
    window.draw(srcLabel);
    window.draw(dstLabel);
    window.draw(srcBox.box);
    window.draw(dstBox.box);
    window.draw(srcBox.text);
    window.draw(dstBox.text);
    window.draw(runBtn);
    window.draw(runText);
    if (hasResult)
      window.draw(outputText);



    window.display();
  }

}
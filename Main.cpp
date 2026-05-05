# include <Siv3D.hpp>

// --- 列挙型と構造体定義 ---
enum class RoomType { Add, Multiply, Sub };

struct GameData {
	int32 currentLevel = 1;
	int32 playerPower = 10;
	double startTime = 0.0;
};

using MyScene = SceneManager<String, GameData>;

// 敵ユニットの定義
struct EnemyUnit {
	RectF rect;
	int32 power;
	RoomType type;
	Color color;
	bool isActive = true;

	void draw(const Font& font) const {
		if (!isActive) return;
		rect.draw(color).drawFrame(2, Palette::White);
		String label = (type == RoomType::Multiply) ? U"x" + Format(power) :
			(type == RoomType::Sub) ? U"-" + Format(power) : U"+" + Format(power);
		font(label).drawAt(rect.center());
	}
};

// --- エフェクト: 紙吹雪 ---
struct Confetti : IEffect {
	Vec2 pos;
	Vec2 velocity;
	double angle = 0.0;
	Color color;
	Confetti(const Vec2& p) : pos(p), velocity(RandomVec2({ -200, 200 }, { -500, -200 })), color(HSV(Random(360.0), 0.7, 0.9)) {}
	bool update(double t) override {
		velocity.y += 400.0 * Scene::DeltaTime();
		pos += velocity * Scene::DeltaTime();
		angle += 5.0 * Scene::DeltaTime();
		if (pos.y > Scene::Size().y) return false;
		RectF{ Arg::center = pos, 12, 6 }.rotated(angle).draw(color);
		return true;
	}
};

// --- ランキング管理 ---
struct ScoreRecord {
	String name;
	int32 level;
	double time;
};


	void SaveScore(const ScoreRecord & r) {
		JSON json = JSON::Load(U"ranking.json");

		// jsonが空（ファイルがない）か、配列でない場合に空の配列として初期化
		if (not json.isArray()) {
			json = JSON::Parse(U"[]"); // 文字列から空の配列として解析させる
		}

		JSON newEntry;
		newEntry[U"name"] = r.name;
		newEntry[U"level"] = r.level;
		newEntry[U"time"] = r.time;

		// これで push_back が動作します
		json.push_back(newEntry);
		json.save(U"ranking.json");
	}

// --- 各シーンの実装 ---

// 1. タイトルシーン
struct Title : MyScene::Scene {
	Title(const InitData& init) : MyScene::Scene { init } {}
	void update() override {
		if (SimpleGUI::Button(U"STRATEGIC START", Scene::Center().movedBy(-100, 50))) {
			getData().startTime = Scene::Time();
			changeScene(U"Game");
		}
	}
	void draw() const override {
		FontAsset(U"TitleFont")(U"NUMERIC TOWER").drawAt(Scene::Center().movedBy(0, -50), Palette::Gold);
	}
};

// 2. ゲーム本編シーン
struct Game : MyScene::Scene {
	Game(const InitData& init) : MyScene::Scene{ init } {
		// コンストラクタで生成関数を呼ぶことで、シーン開始時に敵が作られます
		GenerateFloor();
	}

	Array<EnemyUnit> enemies;
	RectF playerRect{ 100, 400, 60, 60 };
	bool dragging = false;

	void GenerateFloor() {
		enemies.clear();
		int32 base = getData().playerPower;
		enemies << EnemyUnit{ RectF{ 400, 450, 80, 60 }, base - 5, RoomType::Add, Palette::Crimson };
		enemies << EnemyUnit{ RectF{ 400, 350, 80, 60 }, 2, RoomType::Multiply, Palette::Orange };
		enemies << EnemyUnit{ RectF{ 400, 250, 80, 60 }, base * 2, RoomType::Add, Palette::Darkred };
	}

	void update() override {
		// (既存のアップデート処理)
		if (playerRect.mouseOver() && MouseL.down()) dragging = true;
		if (dragging) {
			playerRect.setCenter(Cursor::Pos());
			if (MouseL.up()) {
				dragging = false;
				for (auto& e : enemies) {
					if (e.isActive && playerRect.intersects(e.rect)) {
						playerRect.setCenter(e.rect.center());
						if (getData().playerPower >= e.power || e.type == RoomType::Multiply) {
							if (e.type == RoomType::Add) getData().playerPower += e.power;
							else if (e.type == RoomType::Multiply) getData().playerPower *= e.power;
							e.isActive = false;
							// AudioAsset(U"SE_Win").playOneShot(); // 音源がない場合はコメントアウト
						}
						else {
							changeScene(U"GameOver");
						}
					}
				}
			}
		}
		if (enemies.all([](const EnemyUnit& e) { return !e.isActive; })) {
			changeScene(U"Clear");
		}
	}

	void draw() const override {
		for (const auto& e : enemies) e.draw(FontAsset(U"GameFont"));
		playerRect.draw(Palette::Skyblue).drawFrame(2, Palette::White);
		FontAsset(U"GameFont")(getData().playerPower).drawAt(playerRect.center());
	}
};

// 3. クリア画面
struct Clear : MyScene::Scene {
	Clear(const InitData& init) : MyScene::Scene{ init } {
		// コンストラクタでエフェクトを初期化
		for (int i = 0; i < 100; ++i) effect.add<Confetti>(Scene::Center());
	}

	// draw内でも更新できるように mutable にする
	mutable Effect effect;

	void update() override {
		// updateの中でボタン処理を行いたい場合はここへ
		if (SimpleGUI::Button(U"NEXT FLOOR", Scene::Center().movedBy(-100, 100))) {
			getData().currentLevel++;
			changeScene(U"Game");
		}
		if (SimpleGUI::Button(U"FINISH & SAVE", Scene::Center().movedBy(-100, 160))) {
			changeScene(U"Ranking");
		}
	}

	void draw() const override {
		effect.update(); // エフェクトの更新と描画
		FontAsset(U"TitleFont")(U"STAGE CLEAR").drawAt(Scene::Center());
	}
};

// 4. ランキング/ゲームオーバー等は省略(枠組みは上記と同様)
struct GameOver : MyScene::Scene {
	GameOver(const InitData& init) : MyScene::Scene{ init } {}
	void update() override { if (SimpleGUI::Button(U"TITLE", Scene::Center())) changeScene(U"Title"); }
	void draw() const override { FontAsset(U"TitleFont")(U"DEFEATED").drawAt(Scene::Center(), Palette::Red); }
};

struct Ranking : MyScene::Scene {
	Ranking(const InitData& init) : MyScene::Scene{ init } {}
	mutable TextEditState te;
	void update() override {
		if (SimpleGUI::Button(U"SAVE", Vec2{ 400, 400 })) {
			SaveScore({ te.text, getData().currentLevel, Scene::Time() - getData().startTime });
			changeScene(U"Title");
		}
	}
	void draw() const override {
		SimpleGUI::TextBox(te, Vec2{ 300, 300 });
		FontAsset(U"GameFont")(U"Enter Name to Save").drawAt(400, 250);
	}
};

// --- メイン関数 ---
void Main() {
	Window::SetTitle(U"Numeric Tower Strategy");
	Scene::SetBackground(Palette::Darkslategray);

	// アセット登録
	FontAsset::Register(U"TitleFont", 60, Typeface::Heavy);
	FontAsset::Register(U"GameFont", 25, Typeface::Bold);
	// AudioAsset::Register(U"SE_Win", ...); // 実際にはファイルパスを指定

	MyScene manager;
	manager.add<Title>(U"Title");
	manager.add<Game>(U"Game");
	manager.add<Clear>(U"Clear");
	manager.add<GameOver>(U"GameOver");
	manager.add<Ranking>(U"Ranking");

	while (System::Update()) {
		if (!manager.update()) break;
	}
}

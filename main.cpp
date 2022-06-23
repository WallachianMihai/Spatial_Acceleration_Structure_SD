
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

#define OLC_PGEX_TRANSFORMEDVIEW
#include "olcPGEX_TransformedView.h"

namespace spa
{
    struct rect
    {
        olc::vf2d pos;
        olc::vf2d size;

        rect(const olc::vf2d& p = { 0.0f, 0.0f }, const olc::vf2d& s = { 1.0f, 1.0f }) : pos(p), size(s) {}

        constexpr bool contains(const olc::vf2d& p) const
        {
            return !(p.x < pos.x || p.y < pos.y || p.x >= (pos.x + size.x) || p.y >= (pos.y + size.y));
        }

        constexpr bool contains(const spa::rect& r) const
        {
            return (r.pos.x >= pos.x) && (r.pos.x + r.size.x < pos.x + size.x) &&
                (r.pos.y >= pos.y) && (r.pos.y + r.size.y < pos.y + size.y);
        }

        constexpr bool overlaps(const spa::rect& r) const
        {
            return (pos.x < r.pos.x + r.size.x && pos.x + size.x >= r.pos.x && pos.y < r.pos.y + r.size.y && pos.y + size.y >= r.pos.y);
        }
    };
}

#define MAX_DEPTH 8

template <typename T>
class QuadTree
{
public:
    QuadTree(const spa::rect& size = { {0.0f, 0.0f}, {100.0f, 100.0f} }, const size_t _depth = 0)
    {
        depth = _depth;
        resize(size);
    }

    void resize(const spa::rect& rArea)
    {
        clear();

        r_rect = rArea;
        olc::vf2d vChildSize = r_rect.size / 2.0f;

        r_child =
        {
            spa::rect(r_rect.pos, vChildSize),
            spa::rect({r_rect.pos.x + vChildSize.x, r_rect.pos.y}, vChildSize),
            spa::rect({r_rect.pos.x, r_rect.pos.y + vChildSize.y}, vChildSize),
            spa::rect({r_rect.pos + vChildSize}, vChildSize)
        };
    }

    void clear()
    {
        r_items.clear();
        for (int i = 0; i < 4; i++)
        {
            if (p_child[i])
                p_child[i]->clear();
            p_child[i].reset();
        }
    }

    size_t size() const
    {
        size_t count = r_items.size();

        for (int i = 0; i < 4; i++)
            if (p_child[i])
                count += p_child[i]->size();
        return count;
    }

    void insert(const T& item, const spa::rect& item_size)
    {
        for (int i = 0; i < 4; i++)
        {
            if (r_child[i].contains(item_size))
            {
                if (depth + 1 < MAX_DEPTH)
                {
                    if (!p_child[i])
                    {
                        p_child[i] = std::make_shared<QuadTree<T>>(r_child[i], depth + 1);
                    }
                    p_child[i]->insert(item, item_size);
                    return;
                }
            }
        }
        r_items.push_back({ item_size, item });
    }

    std::list<T> search(const spa::rect& rArea) const
    {
        std::list<T> item_list;
        search(rArea, item_list);
        return item_list;
    }

    void search(const spa::rect& rArea, std::list<T>& item_list) const
    {
        for (const auto& item : r_items)
        {
            if (rArea.overlaps(item.first))
                item_list.push_back(item.second);
        }
        for (int i = 0; i < 4; i++)
        {
            if (p_child[i])
            {
                if (rArea.contains(r_child[i]))
                    p_child[i]->items(item_list);
                else if (r_child[i].overlaps(rArea))
                    p_child[i]->search(rArea, item_list);
            }
        }
    }

    void items(std::list<T>& item_list) const
    {
        for (const auto& item : r_items)
            item_list.push_back(item.second);

        for (int i = 0; i < 4; i++)
            if (p_child[i])
                p_child[i]->items(item_list);
    }

    const spa::rect& area()
    {
        return r_rect;
    }
protected:
    size_t depth = 0;

    spa::rect r_rect;

    std::array<spa::rect, 4> r_child{};

    std::array<std::shared_ptr<QuadTree<T>>, 4> p_child{};

    std::vector<std::pair<spa::rect, T>> r_items; 
};


template <typename T>
class QuadTreeContainer
{
    using TreeContainer = std::list<T>;

protected:
    TreeContainer all_items;
    QuadTree<typename TreeContainer::iterator> root;

public:
    QuadTreeContainer(const spa::rect& size = { {0.0f, 0.0f}, {100.0f, 100.0f} }, const size_t _depth = 0) : root(size)
    {}

    void resize(const spa::rect& rArea)
    {
        root.resize(rArea);
    }

    size_t size()
    {
        return all_items.size();
    }

    bool empty()
    {
        return all_items.empty();
    }

    void clear()
    {
        root.clear();
        all_items.clear();
    }

    typename TreeContainer::iterator begin()
    {
        return all_items.begin();
    }

    typename TreeContainer::iterator end()
    {
        return all_items.end();
    }

    void insert(const T& item, const spa::rect& item_size)
    {
        all_items.push_back(item);
        root.insert(std::prev(all_items.end()), item_size);
    }

    std::list<typename TreeContainer::iterator> search(const spa::rect& rArea)
    {
        std::list<typename TreeContainer::iterator> pointer_list;
        root.search(rArea, pointer_list);
        return pointer_list;
    }

};

class QuadTreeApplication : public olc::PixelGameEngine
{
public:
    QuadTreeApplication()
    {
        sAppName = "QuadTreeApplication";
    }

protected:
    olc::TransformedView tv;

    struct SomeObjectWithArea
    {
        olc::vf2d vPos;
        olc::vf2d vSize;
        olc::Pixel colour;
    };

    std::vector<SomeObjectWithArea> vecObjects;
    QuadTree<SomeObjectWithArea> treeObjects;

    float fArea = 100000.0f;

    bool onQuadTree = false;
public:
    bool OnUserCreate() override
    {
        tv.Initialise({ ScreenWidth(), ScreenHeight() });

        treeObjects.resize(spa::rect({ 0.0f, 0.0f }, { fArea, fArea }));

        auto rand_float = [](const float a, const float b)
        {
            return float(rand()) / float(RAND_MAX) * (b - a) + a;
        };

        for (int i = 0; i < 1000000; i++)
        {
            SomeObjectWithArea ob;
            ob.vPos = { rand_float(0.0f, fArea), rand_float(0.0f, fArea) };
            ob.vSize = { rand_float(0.1f, 100.0f), rand_float(0.1f, 100.0f) };
            ob.colour = olc::Pixel(rand() % 256, rand() % 256, rand() % 256);

            vecObjects.push_back(ob);
            treeObjects.insert(ob, spa::rect(ob.vPos, ob.vSize));
        }

        return true;
    }

    bool OnUserUpdate(float fElapsedTime) override
    {

        tv.HandlePanAndZoom();

        if (GetKey(olc::Key::TAB).bPressed)
            onQuadTree = !onQuadTree;


        spa::rect rScreen = { tv.GetWorldTL(), tv.GetWorldBR() - tv.GetWorldTL() };
        size_t count = 0ULL;
        if (!onQuadTree)
        {

            auto tpStart = std::chrono::system_clock::now();


            for (const auto& object : vecObjects)
            {
                if (rScreen.overlaps({ object.vPos, object.vSize }))
                {
                    tv.FillRectDecal(object.vPos, object.vSize, object.colour);
                    count++;
                }
            }
            std::chrono::duration<float> duration = std::chrono::system_clock::now() - tpStart;

            std::string sOutput = "Linear " + std::to_string(count) + "/" + std::to_string(vecObjects.size()) + " in " + std::to_string(duration.count());
            DrawStringDecal({ 4, 4 }, sOutput, olc::BLACK, { 4.0f, 8.0f });
            DrawStringDecal({ 2, 2 }, sOutput, olc::WHITE, { 4.0f, 8.0f });
        }
        else
        {
            auto tpStart = std::chrono::system_clock::now();
            for (const auto& object : treeObjects.search(rScreen))
            {
                tv.FillRectDecal(object.vPos, object.vSize, object.colour);
                ++count;
            }
            std::chrono::duration<float> duration = std::chrono::system_clock::now() - tpStart;

            std::string sOutput = "Quad Tree " + std::to_string(count) + "/" + std::to_string(vecObjects.size()) + " in " + std::to_string(duration.count());
            DrawStringDecal({ 4, 4 }, sOutput, olc::BLACK, { 4.0f, 8.0f });
            DrawStringDecal({ 2, 2 }, sOutput, olc::WHITE, { 4.0f, 8.0f });
        }

        return true;
    }
};

int main()
{
    QuadTreeApplication demo;
    if (demo.Construct(1280, 960, 1, 1, false, false))
        demo.Start();
    return 0;
}
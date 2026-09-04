final class BookmarksListCellStrategy {
  private enum CellId {
    static let listItem = "BookmarksListCell"
    static let sectionHeader = "SectionHeader"
  }

  /// The cell is passed instead of an index path because the row can be moved or deleted while the
  /// configured cell is alive.
  typealias EditHandlerClosure = (UITableViewCell) -> Void
  var cellEditHandler: EditHandlerClosure?

  func registerCells(_ tableView: UITableView) {
    tableView.register(cell: BookmarksListCell.self)
    tableView.register(UINib(nibName: "BookmarksListSectionHeader", bundle: nil),
                       forHeaderFooterViewReuseIdentifier: CellId.sectionHeader)
  }

  func tableCell(_ tableView: UITableView,
                 for viewModel: IBookmarksListSectionViewModel,
                 at indexPath: IndexPath) -> UITableViewCell {
    switch viewModel {
    case let bookmarksSection as IBookmarksSectionViewModel:
      let bookmark = bookmarksSection.bookmarks[indexPath.row]
      let cell = tableView.dequeueReusableCell(withIdentifier: CellId.listItem, for: indexPath) as! BookmarksListCell
      cell.configure(.bookmark(bookmark, infoAction: { [weak self, weak cell] _ in
        guard let cell else { return }
        self?.cellEditHandler?(cell)
      }))
      return cell
    case let tracksSection as ITracksSectionViewModel:
      let track = tracksSection.tracks[indexPath.row]
      let cell = tableView.dequeueReusableCell(withIdentifier: CellId.listItem, for: indexPath) as! BookmarksListCell
      cell.configure(.bookmark(track, infoAction: { [weak self, weak cell] _ in
        guard let cell else { return }
        self?.cellEditHandler?(cell)
      }))
      return cell
    default:
      fatalError("Unexpected item")
    }
  }

  func headerView(_ tableView: UITableView,
                  for viewModel: IBookmarksListSectionViewModel) -> UITableViewHeaderFooterView {
    let headerView = tableView.dequeueReusableHeaderFooterView(withIdentifier: CellId.sectionHeader)
      as! BookmarksListSectionHeader
    headerView.config(viewModel)
    return headerView
  }
}
